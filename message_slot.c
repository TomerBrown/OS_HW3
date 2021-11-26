
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

The main idea is to implemt the charachter device as a linked list with node that contains the following fields:
    1) Minor Number
    2) Message it self
    3) Message Length
    4) Pointer to the next element

Adding a new slot is very easy.
Searching for the right slot might take some time O(n) as we need to scan element by element in order to find the correct one.

*/


/********************************************************************************************************************
 *                                  Linked List Related Functions and structs
*******************************************************************************************************************/

/*A struct to represent a node in the linked list*/
typedef struct MessageNode {
    unsigned int minorNum;
    char message [MESSAGE_MAX_LEN];
    int len;
    struct MessageNode* next;
} MessageNode;


/*A struct to represent thehe linked list itself*/
typedef struct SlotList {
    MessageNode* first;
    MessageNode* last;
    int len;
} SlotList;

/*A function to initialize an empty list*/
SlotList initList(void){
    SlotList lst;
    lst.first = NULL;
    lst.last = NULL;
    lst.len = 0;
    return lst;
}

/*A function that given all parameters of the nodes append it to the end of the list
* If failed to allocate new memomry as needed returns -1
*/
int appendMessage (SlotList* lst, char* message , int messageLen , unsigned int minorNum){
    int i;
    MessageNode* newMessageNode = (MessageNode*) kmalloc(sizeof(MessageNode),GFP_KERNEL);
    if (newMessageNode == NULL){
        /*todo: Check what to in this case*/
        return -1;
    }

    // Copy content of the message into the new node created
    
    for (i=0; i< messageLen; i++){
        (newMessageNode->message)[i] = message[i];
    }
    //update all other relevant fields of the node
    newMessageNode->len = messageLen;
    newMessageNode->minorNum = minorNum;
    newMessageNode->next = NULL;

    //Finnaly insert the new node to the end of the list
    if (lst->len ==0){
        lst->first = newMessageNode;
        lst->last = newMessageNode;
        lst -> len = 1;
        return SUCCESSFUL;
    }

    //Append is easy O(1) because of the maintnance of the pointer to the last node
    else{
        lst->last->next = newMessageNode;
        lst->last = newMessageNode;
        lst->len = lst->len+1;
        return SUCCESSFUL;
    }
}

/*A function that given a minor Number returns a pointer to the given node in the linked list or NULL if one does not excists in list*/
MessageNode* findByMinorNumber(SlotList* lst, unsigned int minorNum){
    MessageNode* node = lst->first;
    while (node!=NULL){
        if (node->minorNum == minorNum){
            return node;
        }
        node = node->next;
    }
    return NULL;
}

/*A function to free all space alocated for the linked list*/
void free_list(SlotList* lst){
    
    int i;
    MessageNode* node;
    MessageNode* next;

    if (lst->len ==0) {
        return;
    }

    node = lst->first;
    next = node->next;
    
    for (i=0; i< lst->len; i++){
        kfree(node);
        node = next;
        if (node!=NULL){
            next = next->next;
        }
    }

}

/*A Function to print the list and it's element*/ 
void printList(SlotList* lst){
    int i;
    printk("length of list is : %d\n", lst->len);
    if (lst->len ==0){
        printk("Empty List\n");
    }
    else{
        MessageNode* node = lst->first;
        printk("[\n");
        for (i=0; i< lst->len; i++){
            printk("\t{");
            printk("Message: %s , " ,node->message);
            printk("len: %d , " ,node->len);
            printk("Minor Number: %d" ,node->minorNum);
            printk("}");
            if (i<lst->len-1){
                printk(", \n");
            }
            else{
                printk("\n]\n");
            }
            node = node->next;
        }
    }

}


/*************************************************************************************************************************
 *                                            Device Related Functions
 * **********************************************************************************************************************/


/*=======================================  Variables Used Globally by the Module ========================================*/

/*The linked list to store all info*/
static SlotList slots_lst;


/*=========================================== Implemantaion of device functions ==============================================*/

static int device_open( struct inode* inode, struct file*  file ) {
    unsigned int minorNumber;
    MessageNode* messageNode;

    printk("Inside device_open()");


    minorNumber = iminor(inode);
    printk("open - Minor number is: %d",minorNumber);
    messageNode = findByMinorNumber(&slots_lst, minorNumber);

    if (messageNode == NULL){
        printk("open - Did not find a message Node with minor number %d so created a new one",minorNumber);
        //It means that the needed node for minor number doesn't excists - then create a blank one
        if (appendMessage(&slots_lst , "" , 0 , minorNumber)<0){
            printk(KERN_ERR "Error in allocating memory for the new slot");
            return -1;
        }
    }

    if (findByMinorNumber(&slots_lst, minorNumber)!= NULL){
        printk("open was successful! - node is in the Message List");
    }
    else{
        printk("Error in appending the message slot to list");
    }
    return SUCCESSFUL;
}

static ssize_t device_ioctl (struct file* fd , unsigned int control_command , unsigned long commandParameter){
    MessageNode* messageNode;
    int* channelID;

    printk("Inside device_ioctl():  controlcommand = %d, commandParameter = %ld",control_command,commandParameter);
    channelID = kmalloc(1,sizeof(int));
    *channelID = commandParameter;

    if (control_command != MSG_SLOT_CHANNEL){
        printk(KERN_ERR "ioctl isn't MSG_SLOT_CHANNEL\n");
        return -EINVAL;
    }

    if (commandParameter== 0){
        printk(KERN_ERR "ioctl control channel is 0\n");
        return -EINVAL;
    }
    fd->private_data = (void*) (channelID);

    printk("ioctl: minor number is : %ld",commandParameter);
    messageNode = findByMinorNumber(&slots_lst , commandParameter);
    if (messageNode == NULL){
        appendMessage(&slots_lst,"",0,commandParameter);
    }
    printk("ioctl - message node found successfully");

    

    printk("fd->private data is: %d" , *(int*)(fd->private_data));
    return SUCCESSFUL;
}

static ssize_t device_read (struct file* fd, char* buffer , size_t buffer_size, loff_t* offset){
    int minorNumber;
    int i;
    MessageNode* node;

    printk("Inside device_read()");
    if (fd->private_data==NULL){
        return -EINVAL;
    }
    else{
        minorNumber =  *(int*)(fd->private_data);
        if (minorNumber<=0){
            return -EINVAL;
        }
    }
    printk("read - Minor number is: %d",minorNumber);
    node = findByMinorNumber(&slots_lst , minorNumber);

    if (node==NULL){
        //Such file as requested does not exsits
        printk(KERN_ERR "No such slot exists");
        return -EINVAL;
    }
    else{
        printk("Read - Node found successfully");
    }

    if (node->len == 0){
        printk (KERN_ERR "no message to be read inside slot");
        return -EWOULDBLOCK;
    }

    if (buffer_size < node->len){
        printk (KERN_ERR "Not enough space in the buffer for the message");
        return -ENOSPC;
    }

    /*Copy to buffer char by char*/
    for (i=0; i< node->len; i++){
        if (put_user(node->message[i],buffer+i)<0){
            printk(KERN_ERR "Error copying from kernel space to user space \n");
            return -1;
        }
    }
    //At the end i should be exactly how many charachters read.
    printk("Read - read %d letters",i);
    return i;
}

static ssize_t device_write (struct file* fd, const char* buffer , size_t buffer_size, loff_t* offset){
    int minorNumber;
    int i;
    MessageNode* node;

    printk("Inside device_write()");
    if (fd->private_data==NULL){
        if (minorNumber<=0){
            return -EINVAL;
        }
    }
    else{
        minorNumber =  *(int*)(fd->private_data);
    } 
    printk("write- minor number is: %d",minorNumber);
    node = findByMinorNumber(&slots_lst , minorNumber);

    if (buffer_size<= 0 || buffer_size >128){
        return -EMSGSIZE;
    }

    if (node== NULL){
        return -EINVAL;
    }
    else{
        printk("write - node found successfully");
    }

    for (i=0;i<buffer_size;i++){
        if (get_user(node->message[i],&buffer[i])<0){
            printk(KERN_ERR "Error in get_user function call inside device_write");
            return -1;
        }
    }
    node->len = buffer_size;
    printk("Succesfully writen the following.\n\tmessage: %s\n\tlength: %d",node->message,node->len);
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
    slots_lst = initList();
    printk ("Everything Initialized Well! Major number is: %d \n", major);
    return SUCCESSFUL;
}


/*A function to run when the Module finishes (rmmod in command line) 
The Function do the following:
    1) Unregister the char device
    2) Frees the slot list allocated when running
*/
static void messageSlot_cleanup(void){
  printk( "Cleaning up hello module.\n");
  free_list (&slots_lst);
  unregister_chrdev(MAJOR_NUMBER, DEVICE_NAME);
}



/*********************************** Loading the init and exit function to Module *************************************/
module_init(messageSlot_init);
module_exit(messageSlot_cleanup);