#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include <errno.h>
#include <signal.h>
#include <pthread.h>
#include "queue.h"
#include <time.h>

#define USE_AESD_CHAR_DEVICE 1

char *AESD_CHAR_DEVICE = "/dev/aesdchar";

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
char *AESD_SOCKET_DATA = "/var/tmp/aesdsocketdata";


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

void append_timestamp(){
    time_t rawtime;
    struct tm *info;
    char buffer[80];

    time( &rawtime );

    info = localtime( &rawtime );

    strftime(buffer,80,"timestamp:%Y%m%d%H%M%S\n", info);
    printf("%s\n", buffer );

    pthread_mutex_lock(&mutex);
    appendToFile(AESD_SOCKET_DATA, buffer);
    pthread_mutex_unlock(&mutex);
}

bool TIMER_DONE = false;
int TIMER_SLEEP = 10;
void *threadproc(void *arg)
{
    while(!TIMER_DONE)
    {
        sleep(TIMER_SLEEP);
        append_timestamp();
    }
    return 0;
}

typedef struct slist_data_s slist_data_t;
struct slist_data_s {
    int acceptedfd;
    pthread_t thread;
    bool thread_complete;
    SLIST_ENTRY(slist_data_s) entries;
};

void *receive_data(void *args){
    // Receives data over the connection and appends to file
    // /var/tmp/aesdsocketdata, creating this file if it doesn't exist.
    slist_data_t *datap = args;
    int acceptedfd = datap->acceptedfd;
    char* data = (char*) malloc(1);
    memset(data, 0, 1);
    size_t data_len = 0;
    int BUF_SIZE = 1024;
    char* buffer = (char*) malloc(BUF_SIZE);
    memset(buffer, 0, BUF_SIZE);
    ssize_t valread = 0;
    bool recv_data = true;
    while (recv_data){
        valread = recv(acceptedfd, buffer, BUF_SIZE, 0);
        if(valread < 0){
            break;
        }
        if(valread == 0){
            continue;
        }
        data_len += valread;
        data = (char *) realloc(data, (data_len + valread)+1);
        if(data == NULL){
            printf("DATA NOT ALLOCATED!");
            exit(0);
        }
        else{
            // printf("valread %ld = data_len %ld\n", valread, data_len);
        }
        for (ssize_t i = 0; i < valread; i++)
        {
            char c = buffer[i];
            char tmpstr[2];
            tmpstr[0] = c;
            tmpstr[1] = 0;

            strcat (data, tmpstr);
            if (c == '\n')
            {
                printf("Found word: %s", data);
                recv_data=false;

                if(USE_AESD_CHAR_DEVICE){
                    appendToFile(AESD_CHAR_DEVICE, data);
                }
                else{
                    pthread_mutex_lock(&mutex);
                    appendToFile(AESD_SOCKET_DATA, data);
                    pthread_mutex_unlock(&mutex);
                }
                FILE * fp;
                char * line = NULL;
                size_t len = 0;
                ssize_t read;

                if(USE_AESD_CHAR_DEVICE){
                    fp = fopen(AESD_CHAR_DEVICE, "r");
                }
                else{
                    fp = fopen(AESD_SOCKET_DATA, "r");
                }
                if (fp == NULL)
                    exit(EXIT_FAILURE);

                while ((read = getline(&line, &len, fp)) != -1) {
                    // printf("read = %ld\n", read);
                    // printf("acceptedfd = %d\n", acceptedfd);
                    // printf("sending: %s", line);
                    ssize_t size_sent = send(acceptedfd, line, read, 0);
                    if(size_sent == -1){
                        printf("ERROR SENDING\n");
                    }
                    else{
                        // printf("Sent = %ld\n", size_sent);
                    }
                }
                fclose(fp);
                free(line);
                break;
            }
        }
        memset(buffer, 0, BUF_SIZE);
    }
    free(buffer);
    free(data);
    datap->thread_complete = true;

    return args;
}

int sockfd;
struct addrinfo *servinfo;
SLIST_HEAD(slisthead, slist_data_s) head;
void sig_handler(int signum){
    syslog(LOG_INFO, "Caught signal, exiting\n");
    printf("Caught signal, exiting\n");

    if (remove("/var/tmp/aesdsocketdata") == 0){
        printf("Deleted successfully\n");
    }
    else{
        printf("Unable to delete the file\n");
    }

    // delete list
    slist_data_t *datap=NULL;
    while (!SLIST_EMPTY(&head)) {
        datap = SLIST_FIRST(&head);
        pthread_join(datap->thread, NULL);
        SLIST_REMOVE_HEAD(&head, entries);
        free(datap);
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

    // start timer
    if(!USE_AESD_CHAR_DEVICE){
        pthread_t tid;
        pthread_create(&tid, NULL, &threadproc, NULL);
    }

    int acceptedfd;
    char ipv4[INET_ADDRSTRLEN];
    // Listens for and accepts a connection
    if (listen(sockfd, 3) < 0) {
        perror("listen");
        return -1;
    }

    SLIST_INIT(&head);
    slist_data_t *datap=NULL;

    while (1)
    {
        if ((acceptedfd = accept(sockfd, servinfo->ai_addr, &servinfo->ai_addrlen)) < 0) {
            perror("accept");
            return -1;
        }
        syslog(LOG_INFO, "Accepted connection from %s\n", inet_ntop(AF_INET, &servinfo, ipv4, INET_ADDRSTRLEN));
        printf("Accepted connection from %s\n", inet_ntop(AF_INET, &servinfo, ipv4, INET_ADDRSTRLEN));

        datap = malloc(sizeof(slist_data_t));
        datap->acceptedfd = acceptedfd;
        datap->thread_complete = false;

        SLIST_INSERT_HEAD(&head, datap, entries);

        pthread_create(&datap->thread, NULL, receive_data, (void*) datap);
        // openFile("/var/tmp/aesdsocketdata", acceptedfd);

        // remove
        slist_data_t *np_temp = NULL;
        SLIST_FOREACH_SAFE(datap, &head, entries, np_temp){
            if(datap->thread_complete){
                // closing the connected socket
                close(datap->acceptedfd);
                syslog(LOG_INFO, "Closed connection from %s\n", inet_ntop(AF_INET, &servinfo, ipv4, INET_ADDRSTRLEN));
                printf("Closed connection from %s\n", inet_ntop(AF_INET, &servinfo, ipv4, INET_ADDRSTRLEN));
                pthread_join(datap->thread, NULL);
                SLIST_REMOVE(&head, datap, slist_data_s, entries);
                free(datap);
            }
        }
    }

    if (remove("/var/tmp/aesdsocketdata") == 0){
        printf("Deleted successfully\n");
    }
    else{
        printf("Unable to delete the file\n");
    }

    return 0;
}
