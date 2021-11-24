#define MAJOR_NUMBER 240
#define MESSAGE_MAX_LEN 128
#define DEVICE_NAME "message_slot"
#define SUCCESSFUL 0


#undef __KERNEL__
#define __KERNEL__


#undef MODULE
#define MODULE

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kern_levels.h>
#include <linux/ioctl.h>
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

/*A function that given all parameters of the nodes append it to the end of the list*/
int appendMessage (SlotList* lst, char* message , int messageLen , unsigned int minorNum){
    int i;
    MessageNode* newMessageNode = (MessageNode*) kmalloc(sizeof(MessageNode),GFP_ATOMIC);
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


/*****************************************   Variables Used Globally by the program*************************************/

static SlotList slots_lst;

static int device_open( struct inode* inode, struct file*  file ) {
    return SUCCESSFUL;
}

static ssize_t device_ioctl (struct file* fd , unsigned int control_command , unsigned long commandaParameter){
    return SUCCESSFUL;
}

static ssize_t device_read (struct file* fd, char* buffer , size_t buffer_size,  loff_t* offset){
    return SUCCESSFUL;
}

static ssize_t device_write (struct file* fd, const char* buffer , size_t buffer_size,  loff_t* offset){
    return SUCCESSFUL;
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


static void messageSlot_cleanup(void){
  printk( "Cleaning up hello module.\n");
  free_list (&slots_lst);
  unregister_chrdev(MAJOR_NUMBER, DEVICE_NAME);
}


module_init(messageSlot_init);
module_exit(messageSlot_cleanup);