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
#include <linux/ctype.h>

#define DEVICE_NAME "xtea_driver"
#define CLASS_NAME  "xtea_class"

MODULE_LICENSE("GPL");            ///< The license type -- this affects available functionality
MODULE_AUTHOR("Author Name");    ///< The author -- visible when you use modinfo
MODULE_DESCRIPTION("A generic Linux char driver.");  ///< The description -- see modinfo
MODULE_VERSION("0.2");            ///< A version number to inform users

static int    majorNumber;                  ///< Stores the device number -- determined automatically
static char   message[256] = {0};           ///< Memory for the string that is passed from userspace
static short  size_of_message;              ///< Used to remember the size of the string stored
static int    numberOpens = 0;              ///< Counts the number of times the device is opened
static struct class *charClass  = NULL; ///< The device-driver class struct pointer
static struct device *charDevice = NULL; ///< The device-driver device struct pointer

#define XTEA_ROUNDS 32

static void xtea_encipher(u32 v[2], const u32 key[4])
{
        u32 i;
        u32 v0 = v[0];
        u32 v1 = v[1];
        u32 sum = 0;
        const u32 delta = 0x9E3779B9;

        for (i = 0; i < XTEA_ROUNDS; i++) {
                v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^
                      (sum + key[sum & 3]);
                sum += delta;
                v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^
                      (sum + key[(sum >> 11) & 3]);
        }

        v[0] = v0;
        v[1] = v1;
}

static void xtea_decipher(u32 v[2], const u32 key[4])
{
        u32 i;
        u32 v0 = v[0];
        u32 v1 = v[1];
        const u32 delta = 0x9E3779B9;
        u32 sum = delta * XTEA_ROUNDS;

        for (i = 0; i < XTEA_ROUNDS; i++) {
                v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^
                      (sum + key[(sum >> 11) & 3]);
                sum -= delta;
                v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^
                      (sum + key[sum & 3]);
        }

        v[0] = v0;
        v[1] = v1;
}

#define MAX_DATA_SIZE 64
#define MAX_COMMAND_SIZE 256

static char *next_token(char **input)
{
        char *token;

        do {
                token = strsep(input, " \t\n");
        } while (token && token[0] == '\0');

        return token;
}

static int hex_to_bytes(const char *text, u8 *bytes, size_t byte_count)
{
        size_t i;
        int high, low;

        if (strlen(text) != byte_count * 2)
                return -EINVAL;

        for (i = 0; i < byte_count; i++) {
                high = hex_to_bin(text[i * 2]);
                low = hex_to_bin(text[i * 2 + 1]);

                if (high < 0 || low < 0)
                        return -EINVAL;

                bytes[i] = (high << 4) | low;
        }

        return 0;
}

static void bytes_to_hex(const u8 *bytes, size_t byte_count, char *text)
{
        static const char hex[] = "0123456789abcdef";
        size_t i;

        for (i = 0; i < byte_count; i++) {
                text[i * 2] = hex[bytes[i] >> 4];
                text[i * 2 + 1] = hex[bytes[i] & 0x0f];
        }

        text[byte_count * 2] = '\0';
}

static void xtea_crypt_data(u8 *data, size_t data_size,
                            const u32 key[4], bool decrypt)
{
        size_t i;
        u32 block[2];

        for (i = 0; i < data_size; i += 8) {
                block[0] = ((u32)data[i] << 24) |
                           ((u32)data[i + 1] << 16) |
                           ((u32)data[i + 2] << 8) |
                           data[i + 3];

                block[1] = ((u32)data[i + 4] << 24) |
                           ((u32)data[i + 5] << 16) |
                           ((u32)data[i + 6] << 8) |
                           data[i + 7];

                if (decrypt)
                        xtea_decipher(block, key);
                else
                        xtea_encipher(block, key);

                data[i]     = block[0] >> 24;
                data[i + 1] = block[0] >> 16;
                data[i + 2] = block[0] >> 8;
                data[i + 3] = block[0];
                data[i + 4] = block[1] >> 24;
                data[i + 5] = block[1] >> 16;
                data[i + 6] = block[1] >> 8;
                data[i + 7] = block[1];
        }
}

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

static ssize_t dev_read(struct file *filep, char *buffer,
                        size_t len, loff_t *offset)
{
        size_t message_len;

        if (size_of_message == 0)
                return 0;

        if (len < size_of_message)
                return -EINVAL;

        message_len = size_of_message;

        if (copy_to_user(buffer, message, message_len))
                return -EFAULT;

        size_of_message = 0;
        printk(KERN_INFO "XTEA Driver: sent %zu characters to the user\n",
               message_len);

        return message_len;
}

static ssize_t dev_write(struct file *filep, const char *buffer,
                         size_t len, loff_t *offset)
{
        char input[MAX_COMMAND_SIZE];
        char *cursor;
        char *operation;
        char *size_text;
        char *data_text;
        char *extra;
        u32 key[4];
        u8 data[MAX_DATA_SIZE];
        unsigned int data_size;
        int ret;
        int i;
        bool decrypt;

        if (len == 0 || len >= sizeof(input))
                return -EINVAL;

        if (copy_from_user(input, buffer, len))
                return -EFAULT;

        input[len] = '\0';
        cursor = input;

        operation = next_token(&cursor);
        if (!operation)
                return -EINVAL;

        if (!strcmp(operation, "enc"))
                decrypt = false;
        else if (!strcmp(operation, "dec"))
                decrypt = true;
        else
                return -EINVAL;

        for (i = 0; i < 4; i++) {
                char *key_text = next_token(&cursor);

                if (!key_text)
                        return -EINVAL;

                ret = kstrtou32(key_text, 16, &key[i]);
                if (ret)
                        return ret;
        }

        size_text = next_token(&cursor);
        data_text = next_token(&cursor);
        extra = next_token(&cursor);

        if (!size_text || !data_text || extra)
                return -EINVAL;

        ret = kstrtouint(size_text, 10, &data_size);
        if (ret)
                return ret;

        if (data_size == 0 || data_size > MAX_DATA_SIZE ||
            data_size % 8 != 0)
                return -EINVAL;

        ret = hex_to_bytes(data_text, data, data_size);
        if (ret)
                return ret;

        xtea_crypt_data(data, data_size, key, decrypt);
        bytes_to_hex(data, data_size, message);
        size_of_message = strlen(message);

        printk(KERN_INFO "XTEA Driver: %scrypted %u bytes\n",
               decrypt ? "de" : "en", data_size);

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
