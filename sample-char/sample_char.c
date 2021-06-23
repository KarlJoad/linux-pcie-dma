/*
 * Creates a read-write character device that simply echoes its written input to
 * the output, which can be read.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>

#define MODULE_NAME "SAMPLE_CHAR_DEV"
#define DRIVER_DESCRIPTION "Sample Character device"

#define DEVICE_NAME "echo_chardev"
#define BUF_LEN 100 // Length of buffer in bytes.

static int major_num;
static char msg[BUF_LEN];
static char *given_msg;

static struct file_operations fops = {
        .read = 0,
        .write = 0,
        .open = 0,
        .release = 0,
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

module_init(sample_char_init);
module_exit(sample_char_exit);
MODULE_AUTHOR("Karl Hallsby <karl@hallsby.com>");
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_LICENSE("GPL");
