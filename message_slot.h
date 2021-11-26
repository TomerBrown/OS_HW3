#include <linux/ioctl.h>
#define MAJOR_NUMBER 240
#define MESSAGE_MAX_LEN 128
#define DEVICE_NAME "message_slot"
#define SUCCESSFUL 0
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUMBER, 0, unsigned long)