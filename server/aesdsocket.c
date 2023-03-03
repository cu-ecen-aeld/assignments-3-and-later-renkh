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

void appendToFile(char *writefile, char *writestr){
    // syslog(LOG_DEBUG, "Writing %s to %s", writestr, basename(writefile));

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
    char * line = (char*) malloc(20000);
    size_t len = 1024;
    ssize_t read;

    fp = fopen(filename, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    while ((read = getline(&line, &len, fp)) != -1) {
        // printf("Retrieved line of length %zu\n", read);
        char *ptr = (char*) line;
        while (len > 0)
        {
            ssize_t bytes_to_send = 1024;
            if(read < 1024){
                bytes_to_send = read;
            }
            ssize_t i = send(acceptedfd, ptr, bytes_to_send, 0);
            // if (i < 1){
            //     printf("Fail send %s\n", strerror(errno));
            // }
            ptr += i;
            read -= i;
        }

    }

    fclose(fp);
    free(line);
}

char * getFileString(char *filename){
    FILE *fp = fopen(filename, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);
    char buffer[10];
    char *input = 0;
    size_t cur_len = 0;
    while (fgets(buffer, sizeof(buffer), fp) != 0)
    {
        size_t buf_len = strlen(buffer);
        char *extra = realloc(input, buf_len + cur_len + 1);
        if (extra == 0)
            break;
        input = extra;
        strcpy(input + cur_len, buffer);
        cur_len += buf_len;
        // printf("sending: %s\n", buffer);
        // ssize_t i = send(acceptedfd, buffer, 10, 0);
        // if (i < 1){
        //     printf("Fail send %s\n", strerror(errno));
        // }
    }
    // printf("%s [%d]", input, (int)strlen(input));
    return input;
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
        char* data = (char*) malloc(1);
        memset(data, 0, 1);
        size_t data_len = 0;
        int BUF_SIZE = 1024;
        char* buffer = (char*) malloc(BUF_SIZE);
        memset(buffer, 0, BUF_SIZE);
        ssize_t valread = 0;
        while ((valread = recv(acceptedfd, buffer, BUF_SIZE, 0)) > 0){
            data_len += valread;
            data = (char *) realloc(data, (data_len + valread)+1);
            if(data == NULL){
                printf("DATA NOT ALLOCATED!");
                exit(0);
            }
            else{
                // printf("valread %ld = data_len %ld\n", valread, data_len);
            }
            // if(valread == 543){
            //     printf("HERE\n");
            //     printf("data so far: %s\n", data);
            //     printf("data_len=%ld\n", data_len);
            // }
            for (ssize_t i = 0; i < valread; i++)
            {
                char c = buffer[i];
                char tmpstr[2];
                tmpstr[0] = c;
                tmpstr[1] = 0;

                // if(valread == 543){
                //     printf("i=%ld and char=%s\n", i, tmpstr);
                // }

                strcat (data, tmpstr);
                if (c == '\n')
                {
                    printf("Found end of line, whole word is: %s", data);
                    char *filename = "/var/tmp/aesdsocketdata";
                    appendToFile(filename, data);
                    FILE * fp;
                    char * line = NULL;
                    size_t len = 0;
                    ssize_t read;

                    fp = fopen(filename, "r");
                    if (fp == NULL)
                        exit(EXIT_FAILURE);

                    while ((read = getline(&line, &len, fp)) != -1) {
                        ssize_t size_sent = send(acceptedfd, line, read, 0);
                        if(size_sent == -1){
                            printf("ERROR SENDING\n");
                        }
                        else{
                            printf("Sent = %ld\n", size_sent);
                        }
                    }
                    // char *string = getFileString("/var/tmp/aesdsocketdata");
                    // printf("got string: %s\n", string);
                    // ssize_t packets = send(acceptedfd, string, data_len, MSG_NOSIGNAL);
                    // if (packets < 1){
                    //     printf("Fail send %s\n", strerror(errno));
                    // }
                    // else{
                    //     printf("Sent %ld packets\n", packets);
                    // }
                    break;
                }
            }
            memset(buffer, 0, BUF_SIZE);
        }

        // openFile("/var/tmp/aesdsocketdata", acceptedfd);
        // closing the connected socket
        close(acceptedfd);
        syslog(LOG_INFO, "Closed connection from %s\n", inet_ntop(AF_INET, &servinfo, ipv4, INET_ADDRSTRLEN));
        printf("Closed connection from %s\n", inet_ntop(AF_INET, &servinfo, ipv4, INET_ADDRSTRLEN));
        free(buffer);
        free(data);

    }

    if (remove("/var/tmp/aesdsocketdata") == 0){
        printf("Deleted successfully\n");
    }
    else{
        printf("Unable to delete the file\n");
    }

    return 0;
}
