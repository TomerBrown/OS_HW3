#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE

#include "message_slot.h"

// include everything neccessary for the module to run
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kern_levels.h>
#include <linux/string.h> 
#include <linux/errno.h>
#include  <linux/slab.h>


MODULE_LICENSE("GPL");


/*
*********************** A Short Explanation of the main Idea of implemantaion ********************************

The main idea is to implemt the charachter device as a two linked list:
1) The first is for all of the Message Slot the device handels
2) The second is for all the channels opened in the device

The First Linked list contains a pointer to the secoond (hence all the channels it manages)

open(): Creates a new Node in Message Slot List
ioctl(): Creates an new node in the channel_lst2
read() / write(): searches across the lists and find the correct message and handles it accordingly
*/

/*------------------------------------------------------------------------------------------------------
*                                 Structs and linked list components
-------------------------------------------------------------------------------------------------------*/
/*A struct to represent a node in the Channels Linked List
Has the following fields:
1)int channelId 
2)char message [MESSAGE_MAX_LEN]
3)int message_len
4)struct ChannelNode* next
*/
typedef struct ChannelNode{
    int channelId;
    char message [MESSAGE_MAX_LEN];
    int message_len ;
    struct ChannelNode* next;
} ChannelNode;

/*A struct to represent the entire Channels List
*Has the following fields:
1)ChannelNode* first;
2)ChannelNode* last;
3)int len;
*/
typedef struct ChannelsList{
    ChannelNode* first;
    ChannelNode* last;
    int len;    
} ChannelsList;

/*A struct to represent a node that represent a entire device slot *has many channels in it
has the following fieldes:
1) int minorNumber;
2) struct SlotNode* next;
3) struct ChannelsList channels;
*/
typedef struct SlotNode{
    int minorNumber;
    struct SlotNode* next;
    struct ChannelsList channels;
    
} SlotNode;

/*A struct to represent a Linked list of the entire devices operated by our module
has the following fieldes:
1) first 
2) last
3) len
*/
typedef struct SlotList{
    SlotNode* first;
    SlotNode* last;
    int len;
}SlotList;


/*------------------------------------------------------------------------------------------------------
*                                        Lists Related Functions
-------------------------------------------------------------------------------------------------------*/


/*A function that initializes the Slot List (each node represent a single Message Slot)*/
SlotList init_slot_list(void){
    SlotList lst;
    lst.len = 0;
    lst.first = NULL;
    lst.last = NULL;
    return lst;
}

/*Given a pointer to Channel list the function initializes it to default values
!Important! - The function does not return anything - just changes the pointer it gets*/
void init_channelList(ChannelsList* lst){
    lst->len = 0;
    lst->first = NULL;
    lst->last = NULL;
}

/*A function to append a new Slot Message (for unique Minor Number) to the device
On Success return SUCCESFULL , and on fail (Error allocating memory) returns -1
*/
int append_slot (SlotList* lst , int minorNumber){
    SlotNode* slotNode;

    slotNode = kmalloc(sizeof(SlotNode),GFP_KERNEL);
    if (slotNode < 0){
        return -1;
    }

    slotNode->minorNumber = minorNumber;
    slotNode->next = NULL;

    init_channelList(&slotNode ->channels);

    if (lst->len == 0){
        lst->first = slotNode;
        lst-> last = slotNode;
        lst-> len = 1;
    }
    else{
        lst->last->next = slotNode;
        lst->last = slotNode;
        lst-> len =lst->len+1;
    }
    return SUCCESSFUL;
}

/*Given a Slot List and a minor Number returns the Node for the Slot with minor number identical to input*/
SlotNode* find_slot_by_minor(SlotList* lst, int minorNumber){
    int i;
    SlotNode* node;
    node = lst->first;
    for (i=0;i<lst->len;i++){
        if (node->minorNumber == minorNumber){
            return node;
        }
        node = node->next;
    }
    return NULL;
}

/*Given the Slot List (!!!) and a message to append to Slot, it searches for the correct place to append it
IMPORTANT - Errors:
    return -2  | when no slot (with correct minor number) found
    return -1  | when failed to allocate memory
 */
int append_channel(SlotList* lst, int minorNumber , int channelId , char* message , int message_len){
    ChannelNode* channelNode;
    SlotNode* slotNode;
    int i;

    slotNode = find_slot_by_minor(lst,minorNumber);
    if (slotNode== NULL){
        return -2;
    }

    if ((channelNode = kmalloc(sizeof(ChannelNode),GFP_KERNEL))<0){
        return -1;
    }

    channelNode->channelId = channelId;
    channelNode->message_len = message_len;

    for (i=0;i<message_len;i++){
        (channelNode->message)[i] = message[i];
    }
    channelNode->next = NULL;

    if(slotNode->channels.len == 0){
        slotNode->channels.first = channelNode;
        slotNode->channels.last = channelNode;
        slotNode->channels.len =1;
    }
    else{
        slotNode->channels.last->next = channelNode;
        slotNode->channels.last = channelNode;
        slotNode->channels.len = slotNode->channels.len+1;
    }
    return SUCCESSFUL;
}

/*Given the list, the minor number and the ChannelID return the node correspond to the Channel in the right place*/
ChannelNode* find_by_minor_and_channelId (SlotList* lst, int minorNumber , int channelId){
    SlotNode* slotNode;
    ChannelNode* channelNode;
    int i;

    slotNode = find_slot_by_minor(lst , minorNumber);
    if (slotNode==NULL){
        return NULL;
    }

    channelNode = slotNode->channels.first;
    for (i=0; i<slotNode->channels.len;i++){
        if (channelNode->channelId == channelId){
            return channelNode;
        }
        channelNode = channelNode->next;
    }
    return NULL;
}

/*A function that print the list - Espescially for debugging*/
void print_slot_list(SlotList* lst){
    int i;
    int j;
    SlotNode* slotNode;
    ChannelNode* channelNode;

    printk("[\n");
    slotNode = lst->first;
    for (i=0;i< lst->len;i++){
        printk ("{minor number: %d , ",slotNode->minorNumber);
        channelNode = slotNode->channels.first;
        printk ("channels [");
        for (j=0; j<slotNode->channels.len;j++){
            printk ("{Id: %d, Message: %.6s , Message len: %d},",channelNode->channelId , channelNode->message, channelNode->message_len);
            channelNode = channelNode->next;
        }
        printk("]}\n");
        slotNode = slotNode->next;
    }
    printk("]\n");
}

/*Deep free the Channel List (helper for free_slot_list)*/
void free_channel_list(ChannelsList* lst){
    int i;
    ChannelNode* node;
    ChannelNode* next;

    node = lst->first;
    for (i=0;i< lst->len; i++){
        next = node->next;
        kfree (node);
        node = next;
    }
}

/*Deep free the slot list (to avoid memory leakage)*/
void free_slot_list(SlotList* lst){
    int i;
    SlotNode* node;
    SlotNode* next;

    node = lst->first;
    for (i=0;i< lst->len; i++){
        next = node->next;
        free_channel_list(&(node->channels));
        kfree(node);
        node = next;
    }
}




/*------------------------------------------------------------------------------------------------------
*                                        Tests
-------------------------------------------------------------------------------------------------------*/
void test_append_and_print(void){
    SlotList lst;
    printk("--------------------------------------------------------------\n");
    printk("A) Append And Print \n");
    printk("--------------------------------------------------------------\n");
    lst = init_slot_list();
    
    printk("Initiailized Succsesfully");
    append_slot(&lst,0);
    printk("Initiazed minor number 0");
    append_channel(&lst,0,0,"Tomer0",6);
    append_channel(&lst,0,1,"Tomer1",6);
    append_channel(&lst,0,2,"Tomer2",6);
    append_channel(&lst,0,3,"Tomer3",6);
    append_slot(&lst,1);
    printk("Initiazed minor number 1");
    append_channel(&lst,1,0,"Efrat0",6);
    append_channel(&lst,1,1,"Efrat1",6);
    append_channel(&lst,1,2,"Efrat2",6);
    append_channel(&lst,1,3,"Efrat3",6);
    append_slot(&lst,2);
    printk("Initiazed minor number 2");
    append_channel(&lst,2,0,"Niv0",4);
    append_channel(&lst,2,1,"Niv1",4);
    append_channel(&lst,2,2,"Niv2",4);
    append_channel(&lst,2,3,"Niv3",4);
    

    append_slot(&lst,3);
    printk("Initiazed minor number 3");
    append_channel(&lst,3,0,"David0",6);
    append_channel(&lst,3,1,"David1",6);
    append_channel(&lst,3,2,"David2",6);
    append_channel(&lst,3,3,"David3",6);
    append_channel(&lst,3,4,"David4",6);
    append_channel(&lst,3,5,"David5",6);
    append_channel(&lst,3,6,"David6",6);
    append_channel(&lst,3,7,"David7",6);
    append_channel(&lst,3,8,"David8",6);
    append_channel(&lst,3,9,"David9",6);
    append_channel(&lst,3,10,"Davi10",6);
    append_channel(&lst,3,11,"Davi11",6);
    append_channel(&lst,3,12,"Davi12",6);
    append_channel(&lst,3,13,"Davi13",6);
    print_slot_list(&lst);

    printk("--------------------------------------------------------------\n");
    printk("B) Find Based on minor and ChannelID\n");
    printk("--------------------------------------------------------------\n");

    printk("1)Should print Tomer3 | Result : %.6s\n", find_by_minor_and_channelId(&lst,0,3)->message);
    printk("2)Should print Efrat3 | Result : %.6s\n", find_by_minor_and_channelId(&lst,1,3)->message);
    printk("3)Should print Niv3 | Result : %.6s\n", find_by_minor_and_channelId(&lst,2,3)->message);
    printk("4)Should print Tomer0 | Result : %.6s\n", find_by_minor_and_channelId(&lst,0,0)->message);
    printk("5)Should print Efrat1 | Result : %.6s\n", find_by_minor_and_channelId(&lst,1,1)->message);
    printk("6)Should print Niv2 | Result : %.6s\n", find_by_minor_and_channelId(&lst,2,2)->message);
    printk("7)Should print Niv3 | Result : %.6s\n", find_by_minor_and_channelId(&lst,2,3)->message);
    printk("8)Should print Tomer1 | Result : %.6s\n", find_by_minor_and_channelId(&lst,0,1)->message);
    printk("9)Should print 1 (=null) | result: %d\n" , find_by_minor_and_channelId(&lst,4,0)==NULL);
    printk("10)Should print 1 (=null) | result: %d\n" , find_by_minor_and_channelId(&lst,0,4)==NULL);
    printk("11)Should print 1 (=null) | result: %d\n" , find_by_minor_and_channelId(&lst,4,4)==NULL);
    printk("12)Should print 1 (=null) | result: %d\n" , find_by_minor_and_channelId(&lst,100,100)==NULL);


    printk("--------------------------------------------------------------\n");
    printk("C) Free List! - Should not print anythig\n");
    printk("--------------------------------------------------------------\n");
    free_slot_list(&lst);


}



/*************************************************************************************************************************
 *                                            Device Related Functions
 * **********************************************************************************************************************/


/*=======================================  Variables Used Globally by the Module ========================================*/

/*The linked list to store all info*/
static SlotList slots_lst;

/*=========================================== Implemantaion of device functions ==============================================*/

static int device_open( struct inode* inode, struct file*  file ) {
    int minorNumber;
    SlotNode* slotNode;

    minorNumber = iminor(inode);
    slotNode = find_slot_by_minor(&slots_lst , minorNumber);

    if (slotNode==NULL){
        if (append_slot(&slots_lst,minorNumber)<0){
            printk(KERN_ERR "Error: Could not open a new Slot with minor number %d",minorNumber);
            return -1;
        }
    }
    return SUCCESSFUL;
}

static ssize_t device_ioctl (struct file* fd , unsigned int control_command , unsigned long commandParameter){
    int minorNumber;
    int* channelId;
    ChannelNode* channelNode;

    if (control_command!= MSG_SLOT_CHANNEL){
        printk(KERN_ERR "ioctl isn't MSG_SLOT_CHANNEL\n");
        return -EINVAL;
    }
    if (commandParameter== 0){
        printk(KERN_ERR "ioctl control channel is 0\n");
        return -EINVAL;
    }

    channelId = kmalloc(sizeof(int),GFP_KERNEL);
    *channelId = commandParameter;
    fd->private_data = (void*) (channelId);
    minorNumber = iminor(fd->f_inode);

    channelNode = find_by_minor_and_channelId(&slots_lst , minorNumber , commandParameter);
    if (channelNode==NULL){
        if (append_channel(&slots_lst, minorNumber, commandParameter,"",0)<0){
            printk(KERN_ERR "ioctl control could not initialize channel 0\n");
            return -1;
        }
    }
    return SUCCESSFUL;
}

static ssize_t device_read (struct file* fd, char* buffer , size_t buffer_size, loff_t* offset){
    int minorNumber;
    int channelId;
    int i;
    ChannelNode* channelNode;

    if (fd->private_data==NULL){
        printk(KERN_ERR "Error: No ioctl has been done before read");
        return -EINVAL;
    }
    channelId = *(int*) fd->private_data;
    minorNumber = iminor(fd->f_inode);
    
    channelNode = find_by_minor_and_channelId(&slots_lst,minorNumber,channelId);
    if (channelNode==NULL || channelNode->message_len ==0){
        return -EWOULDBLOCK;
    }
    if (channelNode->message_len > buffer_size){
        return -ENOSPC;
    }
    for (i=0; i<channelNode->message_len;i++){
        if (put_user(channelNode->message[i],& (buffer[i]))<0){
            printk(KERN_ERR "Error copying from kernel space to user space \n");
            return -1;
        }
    }
    return i;
}

static ssize_t device_write (struct file* fd, const char* buffer , size_t buffer_size, loff_t* offset){
    int i;
    int minorNumber;
    int channelId;
    ChannelNode* channelNode;

    if (fd->private_data==NULL){
        printk(KERN_ERR "Error: No ioctl has been done before write");
        return -EINVAL;
    }

    if (buffer_size>MESSAGE_MAX_LEN || buffer_size<=0){
        return -EMSGSIZE;
    }

    channelId = * (int *) (fd->private_data);
    minorNumber = iminor(fd->f_inode);

    channelNode = find_by_minor_and_channelId(&slots_lst , minorNumber ,channelId);
    if (channelNode==NULL){
        printk(KERN_ERR "Error: No ioctl has been done before write");
        return -1;
    }
    
    for (i=0;i<buffer_size;i++){
        if (get_user(channelNode->message[i], &(buffer[i]))<0){
            printk(KERN_ERR "Error copying from user space to kernel space \n");
            return -1;
        }
    }
    channelNode->message_len = buffer_size;

    return i;
}


/* Defining the File Operations Struct to use by register device with all of these functionalities*/
struct file_operations Fops =
{
  .owner	  = THIS_MODULE, 
  .read           = device_read,
  .write          = device_write,
  .open           = device_open,
  .unlocked_ioctl = device_ioctl,
};
    

/*A function to run when Module First initialized 
    Goals of the function:
        1) Register the device with all it's functionalities
        2) Initialize the linked list that will be used by the module

    * If any issue accured while doing so -> log it to the kernel log*/
static int messageSlot_init(void){
    int major = register_chrdev (MAJOR_NUMBER,DEVICE_NAME,&Fops);
    if (major < 0){
        printk(KERN_ERR "Failed to initialize module major number is :%d\n" , major);
        return -1;
    }
    slots_lst = init_slot_list();
    return SUCCESSFUL;
}


/*A function to run when the Module finishes (rmmod in command line) 
The Function do the following:
    1) Unregister the char device
    2) Frees the slot list allocated when running
*/
static void messageSlot_cleanup(void){
    unregister_chrdev(MAJOR_NUMBER, DEVICE_NAME);
    free_slot_list(&slots_lst);
}



/*********************************** Loading the init and exit function to Module *************************************/
module_init(messageSlot_init);
module_exit(messageSlot_cleanup);