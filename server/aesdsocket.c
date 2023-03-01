#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <libgen.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>

void createFile(char *writefile, char *writestr){
    syslog(LOG_DEBUG, "Writing %s to %s", writestr, basename(writefile));

    FILE *fp = fopen(writefile, "a");
    if (fp == NULL) {
        syslog(LOG_ERR, "Error opening file: %s", writefile);
        exit(1);
    }

    fputs(writestr, fp);
    if (ferror(fp)) {
        syslog(LOG_ERR, "Error writing file: %s", writestr);
        exit(1);
    }

    fclose(fp);
}


bool send_all(int socket, void *buffer, ssize_t length)
{
    char *ptr = (char*) buffer;
    while (length > 0)
    {
        ssize_t bytes_to_send = 1024;
        if(length < 1024){
            bytes_to_send = length;
        }
        ssize_t i = send(socket, ptr, bytes_to_send, 0);
        if (i < 1){
            printf("Fail send %s\n", strerror(errno));
            return false;
        }
        ptr += i;
        length -= i;
    }
    return true;
}

void openFile(char *filename, int acceptedfd){
    FILE * fp;
    char * line = (char*) malloc(1024);
    size_t len = 1024;
    ssize_t read;

    fp = fopen(filename, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    while ((read = getline(&line, &len, fp)) != -1) {
        printf("Retrieved line of length %zu\n", read);
        if(send_all(acceptedfd, line, read)){
            // printf("Sent out: %s", line);
            continue;
        }
        else{
            printf("DID NOT SEND: %ld bytes\n", read);
        }
    }

    fclose(fp);
    free(line);
}

int sockfd;
struct addrinfo *servinfo;
void sig_handler(int signum){
    syslog(LOG_INFO, "Caught signal, exiting\n");
    printf("Caught signal, exiting\n");
    if (remove("/var/tmp/aesdsocketdata") == 0){
        printf("Deleted successfully\n");
    }
    else{
        printf("Unable to delete the file\n");
    }
    // closing the listening socket
    freeaddrinfo(servinfo);
    shutdown(sockfd, SHUT_RDWR);
    exit(signum);
}

int main(int argc, char *argv[])
{
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);
    bool daemon_mode = false;
    if(argc == 2){
        if (strcmp(argv[1], "-d") == 0)
        {
            daemon_mode = true;
        }

    }
    // Opens a stream socket bound to port 9000, failing and returning -1 if any of
    // the socket connection steps fail.

    // bind to port 9000
    int status;
    struct addrinfo hints;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if((status = getaddrinfo(NULL, "9000", &hints, &servinfo)) != 0){
        printf("getaddrinfo error: %s\n", gai_strerror(status));
    }


    // Create a socket file descriptor
    if ((sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, 0)) == -1) {
        perror("socket failed");
        return -1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0){
        perror("setsockopt(SO_REUSEADDR) failed");
    }

    int bindrv = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (bindrv < 0) {
        perror("bind failed");
        return -1;
    }

    if(daemon_mode && bindrv == 0){
        int pid = fork();
        if(pid > 0){
            exit(0);
        }
    }

    int acceptedfd;
    char ipv4[INET_ADDRSTRLEN];
    // Listens for and accepts a connection
    if (listen(sockfd, 3) < 0) {
        perror("listen");
        return -1;
    }
    while (1)
    {
        if ((acceptedfd = accept(sockfd, servinfo->ai_addr, &servinfo->ai_addrlen)) < 0) {
            perror("accept");
            return -1;
        }
        // syslog("Accepted connection from xxx");
        syslog(LOG_INFO, "Accepted connection from %s\n", inet_ntop(AF_INET, &servinfo, ipv4, INET_ADDRSTRLEN));
        printf("Accepted connection from %s\n", inet_ntop(AF_INET, &servinfo, ipv4, INET_ADDRSTRLEN));


        // Receives data over the connection and appends to file
        // /var/tmp/aesdsocketdata, creating this file if it doesn't exist.
        char* data = (char*) malloc(1024);
        memset(data, 0, 1024);
        ssize_t data_len = 1024;
        char* buffer = (char*) malloc(1024);
        memset(buffer, 0, 1024);
        int valread = 0;
        while ((valread = recv(acceptedfd, buffer, 1024, 0)) > 0){
            data = (char *) realloc(data, (data_len + valread));
            data_len += valread;
            for (ssize_t i = 0; i < valread; i++)
            {
                char c = buffer[i];
                char tmpstr[2];
                tmpstr[0] = c;
                tmpstr[1] = 0;

                // resize the data array
                strcat (data, tmpstr);
                if (c == '\n')
                {
                    printf("Found end of line, whole word is: %s", data);
                    createFile("/var/tmp/aesdsocketdata", data);
                    openFile("/var/tmp/aesdsocketdata", acceptedfd);
                    break;
                }
            }
            memset(buffer, 0, 1024);
        }

        // openFile("/var/tmp/aesdsocketdata", acceptedfd);
        // closing the connected socket
        close(acceptedfd);
        syslog(LOG_INFO, "Closed connection from %s\n", inet_ntop(AF_INET, &servinfo, ipv4, INET_ADDRSTRLEN));
        printf("Closed connection from %s\n", inet_ntop(AF_INET, &servinfo, ipv4, INET_ADDRSTRLEN));
        free(buffer);
        free(data);
    }

    return 0;
}
