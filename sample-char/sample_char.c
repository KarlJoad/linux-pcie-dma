/*
 * Creates a read-write character device that simply echoes its written input to
 * the output, which can be read.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h> // For put_user and get_user

#define MODULE_NAME "SAMPLE_CHAR_DEV"
#define DRIVER_DESCRIPTION "Sample Character device"

#define DEVICE_NAME "echo_chardev"
#define BUF_LEN 100 // Length of buffer in bytes.

static int major_num;
static char msg[BUF_LEN];
static char *msg_last_char; // Points to last character in string.

static int sample_char_open(struct inode *inode, struct file *filep);
static int sample_char_release(struct inode *inode, struct file *filep);
static ssize_t sample_char_read(struct file *filep, char *buffer, size_t length,
                                loff_t *offset);
static ssize_t sample_char_write(struct file *filep, const char *buffer,
                                 size_t length, loff_t *offset);
static loff_t sample_char_llseek(struct file *filep, loff_t off, int whence);

static struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = sample_char_open,
        .release = sample_char_release,
        .read = sample_char_read,
        .write = sample_char_write,
        .llseek = sample_char_llseek,
};

static int __init sample_char_init(void) {
        printk(KERN_INFO "Initializing sample character device driver\n");

        /* Register a character device with the kernel. Creates a new file in
         * /proc/devices with the name DEVICE_NAME. Also hooks the file_operations
         * struct into the kernel for the operations supported by character
         * devices.
         * Because 0 was passed, the character device will be given a "random"
         * device number. Not technically random, but the first non-used major
         * device number. */
        major_num = register_chrdev(0, DEVICE_NAME, &fops);
        if(major_num < 0) {
                printk(KERN_ALERT "Registering char device %s filed with %d\n", DEVICE_NAME, major_num);
                return major_num;
        }

        return 0;
}

static void __exit sample_char_exit(void) {
        unregister_chrdev(major_num, DEVICE_NAME);
        printk(KERN_INFO "Unregistered %s char device. Unloading module.\n", DEVICE_NAME);
}

/* The function passed to the open field of the file_operations struct should
 * set everything up for the file to be used. This means bringing the seek pointer
 * to a certain file, setting up device minor numbers, allocating memory space
 * for the device file's private information, and so on. */
static int sample_char_open(struct inode *inode, struct file *filep) {
        char *working_string = "Hello World\0";
        unsigned int i;
        printk(KERN_INFO "Opened the example character device file\n");
        for(i = 0; i < 12; i++) {
                msg[i] = working_string[i];
                printk(KERN_INFO "open: i=%du, @ 0x%p\n", i, &(msg[i]));
        }
        printk(KERN_INFO "Before: msg @ %p, msg_last_char @ %p\n", msg, msg_last_char);
        msg_last_char = &(msg[10]); // Pointer math, go 10 chars past msg
        printk(KERN_INFO "After: msg @ %p, msg_last_char @ %p\n", msg, msg_last_char);
        return 0;
}

/* The function passed to the release field of the file_operations struct should
 * clean everything up when this instance of the file being opened is closed.
 * This will involve kfree-ing everything that was allocated in the open
 * function. */
static int sample_char_release(struct inode *inode, struct file *filep) {
        printk(KERN_INFO "Closed the example character device file\n");
        msg_last_char = 0;
        return 0;
}

/* Called with the read call of file_operations struct is used. This device
 * behaves like a message queue, so the first string that is placed into this
 * device is preserved until it is read. After the read, the data is no longer
 * accessible. */
static ssize_t sample_char_read(struct file *filep, char *buffer, size_t length,
                                loff_t *offset) {
        ssize_t ret;
        msg_last_char = msg_last_char + 10;
        /* If the buffer pointer is the same address as the buffer, then whatever
         * used to be stored here (if anything) has already been returned in an
         * earlier read call. Do nothing here, returning 0 that nothing happened. */
        printk(KERN_INFO "Entered %s for reading\n", DEVICE_NAME);
        printk(KERN_INFO "read: msg @ %p, msg_last_char @ %p\n", msg, msg_last_char);
        if(msg_last_char == msg) {
                return 0;
        }

        printk(KERN_INFO "Copying %s to user buffer @ %p", msg, buffer);
        ret = copy_to_user(msg, buffer, length);

        /* If reading less than the amount of information written to the device,
         * the whole original message is lost (reset msg_last_char to msg's
         * location). */
        msg_last_char = msg;

        /* To get the number of bytes transferred, subtract the current memory
         * address of iter from msg (the base). */
        return ret;
}

static ssize_t sample_char_write(struct file *filep, const char *buffer,
                                 size_t length, loff_t *offset) {
        ssize_t ret;
        /* If the buffer pointer is at the end of the buffer, then buffer is full
         * and we cannot safely write. */
        if(msg_last_char == (msg + (BUF_LEN - 1))) {
                return 0;
        }

        ret = copy_from_user(msg, buffer, length);
        printk(KERN_INFO "Writing: %s", buffer);
        msg_last_char = msg + (length - 1);

        /* To get the number of bytes transferred, subtract the current memory
         * address of iter from msg (the base). */
        return ret;
}

/* Regardless of what is provided, we just reset the msg_last_char to the beginning
 * of the message buffer. */
static loff_t sample_char_llseek(struct file *filep, loff_t off, int whence) {
        msg_last_char = msg;
        return 0;
}

module_init(sample_char_init);
module_exit(sample_char_exit);
MODULE_AUTHOR("Karl Hallsby <karl@hallsby.com>");
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_LICENSE("GPL");
