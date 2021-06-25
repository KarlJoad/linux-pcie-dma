#include "chardev.h"

static int major_num;

static int fpga_char_open(struct inode *inode, struct file *filep);
static int fpga_char_release(struct inode *inode, struct file *filep);
static ssize_t fpga_char_read(struct file *filep, char *buffer, size_t length,
                              loff_t *offset);
static ssize_t fpga_char_write(struct file *filep, const char *buffer,
                               size_t length, loff_t *offset);
static int fpga_char_ioctl(struct inode *inode, struct file *filep, unsigned int,
                           unsigned long);

static const struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = fpga_char_open,
        .release = fpga_char_release,
        .read = fpga_char_read,
        .write = fpga_char_write,
};

/* The function passed to the open field of the file_operations struct should
 * set everything up for the file to be used. This means bringing the seek pointer
 * to a certain file, setting up device minor numbers, allocating memory space
 * for the device file's private information, and so on. */
static int fpga_char_open(struct inode *inode, struct file *filep)
{
        // NOTE: llseek is NOT supported by this device. Call appropriately.
        printk(KERN_INFO "Opened the example character device file\n");
        return 0;
}

/* The function passed to the release field of the file_operations struct should
 * clean everything up when this instance of the file being opened is closed.
 * This will involve kfree-ing everything that was allocated in the open
 * function. */
static int fpga_char_release(struct inode *inode, struct file *filep)
{
        printk(KERN_INFO "Closed the example character device file\n");
        return 0;
}

/* Called with the read call of file_operations struct is used. This device
 * behaves like a message queue, so the first string that is placed into this
 * device is preserved until it is read. After the read, the data is no longer
 * accessible. */
static ssize_t fpga_char_read(struct file *filep, char *buffer, size_t length,
                                loff_t *offset)
{
        // NOTE: Memory pointers are unsigned long (8 bytes, 64 bits, on amd64).
        return length;
}

static ssize_t fpga_char_write(struct file *filep, const char *buffer,
                                 size_t length, loff_t *offset)
{
        // NOTE: Memory pointers are unsigned long (8 bytes, 64 bits, on amd64).
        return length;
}
