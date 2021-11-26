#include "message_slot.h"

#include <stdlib.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <string.h>
#include<fcntl.h> 
#include <unistd.h>

int main (int argc ,char *argv[]){
    char* message = calloc(128,sizeof(char));
    char* slot_path = argv[1];
    int channel_id = atoi(argv[2]);
    

    int fd = open(slot_path,O_RDWR);
    if (fd<0){
        perror("Could not open the file");
        return 1;
    }
    
    ioctl(fd,MSG_SLOT_CHANNEL,channel_id);
    read(fd, message, MESSAGE_MAX_LEN);
    printf("%s",message);
    close (fd);
    free(message);
    return 0;
}