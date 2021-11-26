#include "message_slot.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include<fcntl.h> 
#include <unistd.h>
#include <errno.h>

int main (int argc ,char *argv[]){
    char* slot_path = argv[1];
    int channel_id = atoi(argv[2]);
    char* message_to_pass = argv[3];
    int message_len = strlen(message_to_pass);


    int fd = open(slot_path,O_RDWR);
    if (fd<0){
        perror("Could not open the file");
        return 1;
    }

    int ioc = ioctl(fd,MSG_SLOT_CHANNEL,channel_id);
    if (ioc<0){
        printf ("Error in ioctl() %s \n", strerror(errno));
    }
    write(fd, message_to_pass, message_len);
    close (fd);
    return 0;
}