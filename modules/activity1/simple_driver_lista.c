/**
 * @brief   An introductory character driver. This module maps to /dev/simple_driver and
 * comes with a helper C program that can be run in Linux user space to communicate with
 * this the LKM.
 *
 * Modified from Derek Molloy (http://www.derekmolloy.ie/)
 */

#include <linux/init.h>           // Macros used to mark up functions e.g. __init __exit
#include <linux/module.h>         // Core header for loading LKMs into the kernel
#include <linux/device.h>         // Header to support the kernel Driver Model
#include <linux/kernel.h>         // Contains types, macros, functions for the kernel
#include <linux/fs.h>             // Header for the Linux file system support
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#define  DEVICE_NAME "simple_driver" ///< The device will appear at /dev/simple_driver using this value
#define  CLASS_NAME  "simple_class"        ///< The device class -- this is a character device driver

MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Author Name");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A generic Linux char driver.");  ///< The description -- see modinfo
MODULE_VERSION("0.2");            ///< A version number to inform users

static int    majorNumber;                  ///< Stores the device number -- determined automatically
#define MAX_MESSAGE_SIZE 4096
#define MESSAGE_SUFFIX_SIZE 32

struct message_entry {
        struct list_head list;
        size_t length;
        char data[];
};

static LIST_HEAD(message_queue);
static DEFINE_MUTEX(message_mutex);
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class *charClass  = NULL; ///< The device-driver class struct pointer
static struct device *charDevice = NULL; ///< The device-driver device struct pointer

// The prototype functions for the character driver -- must come before the struct definition
static int     dev_open(struct inode *, struct file *);
static int     dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

/** @brief Devices are represented as file structure in the kernel. The file_operations structure from
 *  /linux/fs.h lists the callback functions that you wish to associated with your file operations
 *  using a C99 syntax structure. char devices usually implement open, read, write and release calls
 */
static struct file_operations fops =
{
	.open = dev_open,
	.read = dev_read,
	.write = dev_write,
	.release = dev_release,
};


/** @brief The LKM initialization function
 *  The static keyword restricts the visibility of the function to within this C file. The __init
 *  macro means that for a built-in driver (not a LKM) the function is only used at initialization
 *  time and that it can be discarded and its memory freed up after that point.
 *  @return returns 0 if successful
 */
static int __init simple_init(void){
	printk(KERN_INFO "Simple Driver: Initializing the LKM\n");

	// Try to dynamically allocate a major number for the device -- more difficult but worth it
	majorNumber = register_chrdev(0, DEVICE_NAME, &fops);
	if (majorNumber<0){
		printk(KERN_ALERT "Simple Driver failed to register a major number\n");
		return majorNumber;
	}
	
	printk(KERN_INFO "Simple Driver: registered correctly with major number %d\n", majorNumber);

	// Register the device class
	charClass = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(charClass)){                // Check for error and clean up if there is
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "Simple Driver: failed to register device class\n");
		return PTR_ERR(charClass);          // Correct way to return an error on a pointer
	}
	
	printk(KERN_INFO "Simple Driver: device class registered correctly\n");

	// Register the device driver
	charDevice = device_create(charClass, NULL, MKDEV(majorNumber, 0), NULL, DEVICE_NAME);
	if (IS_ERR(charDevice)){               // Clean up if there is an error
		class_destroy(charClass);           // Repeated code but the alternative is goto statements
		unregister_chrdev(majorNumber, DEVICE_NAME);
		printk(KERN_ALERT "Simple Driver: failed to create the device\n");
		return PTR_ERR(charDevice);
	}
	
	printk(KERN_INFO "Simple Driver: device class created correctly\n"); // Made it! device was initialized
		
	return 0;
}


/** @brief The LKM cleanup function
 *  Similar to the initialization function, it is static. The __exit macro notifies that if this
 *  code is used for a built-in driver (not a LKM) that this function is not required.
 */
static void __exit simple_exit(void){
	device_destroy(charClass, MKDEV(majorNumber, 0));     // remove the device
	class_unregister(charClass);                          // unregister the device class
	class_destroy(charClass);                             // remove the device class
	unregister_chrdev(majorNumber, DEVICE_NAME);             // unregister the major number
       {
                struct message_entry *entry, *next;

                mutex_lock(&message_mutex);
                list_for_each_entry_safe(entry, next, &message_queue, list) {
                        list_del(&entry->list);
                        kfree(entry);
                }
                mutex_unlock(&message_mutex);
        }
	printk(KERN_INFO "Simple Driver: goodbye from the LKM!\n");
}


/** @brief The device open function that is called each time the device is opened
 *  This will only increment the numberOpens counter in this case.
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_open(struct inode *inodep, struct file *filep){
	numberOpens++;
	printk(KERN_INFO "Simple Driver: device has been opened %d time(s)\n", numberOpens);
	return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
        struct message_entry *entry;
        size_t message_len;

        mutex_lock(&message_mutex);

        if (list_empty(&message_queue)) {
                mutex_unlock(&message_mutex);
                return 0;
        }

        entry = list_first_entry(&message_queue, struct message_entry, list);

        if (len < entry->length) {
                mutex_unlock(&message_mutex);
                return -EINVAL;
        }

        if (copy_to_user(buffer, entry->data, entry->length)) {
                mutex_unlock(&message_mutex);
                return -EFAULT;
        }

        message_len = entry->length;
        list_del(&entry->list);
        mutex_unlock(&message_mutex);

        printk(KERN_INFO "Simple Driver: sent %zu characters to the user\n",
               message_len);
        kfree(entry);
        return message_len;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
        struct message_entry *entry;
        char *input;

        if (len == 0)
                return 0;

        if (len > MAX_MESSAGE_SIZE)
                return -EMSGSIZE;

        input = kmalloc(len + 1, GFP_KERNEL);
        if (!input)
                return -ENOMEM;

        if (copy_from_user(input, buffer, len)) {
                kfree(input);
                return -EFAULT;
        }
        input[len] = '\0';

        entry = kmalloc(sizeof(*entry) + len + MESSAGE_SUFFIX_SIZE, GFP_KERNEL);
        if (!entry) {
                kfree(input);
                return -ENOMEM;
        }

        entry->length = scnprintf(entry->data, len + MESSAGE_SUFFIX_SIZE,
                                  "%.*s(%zu letters)", (int)len, input, len);
        kfree(input);

        mutex_lock(&message_mutex);
        list_add_tail(&entry->list, &message_queue);
        mutex_unlock(&message_mutex);

        printk(KERN_INFO "Simple Driver: received %zu characters from the user\n",
               len);
        return len;
}

/** @brief The device release function that is called whenever the device is closed/released by
 *  the userspace program
 *  @param inodep A pointer to an inode object (defined in linux/fs.h)
 *  @param filep A pointer to a file object (defined in linux/fs.h)
 */
static int dev_release(struct inode *inodep, struct file *filep){
	printk(KERN_INFO "Simple Driver: device successfully closed\n");
	return 0;
}

/** @brief A module must use the module_init() module_exit() macros from linux/init.h, which
 *  identify the initialization function at insertion time and the cleanup function (as
 *  listed above)
 */
module_init(simple_init);
module_exit(simple_exit);
