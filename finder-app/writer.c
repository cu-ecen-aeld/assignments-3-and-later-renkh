#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>

void createFile(char *writefile, char *writestr){
    syslog(LOG_DEBUG, "Writing %s to %s", writestr, basename(writefile));

    FILE *fp = fopen(writefile, "w+");
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

int main(int argc, char *argv[]) {
    openlog(argv[0], LOG_PERROR | LOG_PID, LOG_USER);

    if(argc != 3){
        syslog(LOG_ERR, "Usage: %s [writefile] [writestr]", argv[0]);
        exit(1);
    }

    createFile(argv[1], argv[2]);
    closelog();

    return 0;
}
