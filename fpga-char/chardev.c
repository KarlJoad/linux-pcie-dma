#include "chardev.h"

static int fpga_char_open(struct inode *inode, struct file *filep);
static int fpga_char_release(struct inode *inode, struct file *filep);
static ssize_t fpga_char_read(struct file *filep, char *buffer, size_t length,
                              loff_t *offset);
static ssize_t fpga_char_write(struct file *filep, const char *buffer,
                               size_t length, loff_t *offset);
static long fpga_char_ioctl(struct file *filep, unsigned int i, unsigned long j);

static const struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = fpga_char_open,
        .release = fpga_char_release,
        .read = fpga_char_read,
        .write = fpga_char_write,
        .unlocked_ioctl = fpga_char_ioctl,
};

static struct fpga_char_device_data {
        struct device *fpga_device;
        struct cdev cdev;
} fpga_dev_data;

/* The character driver's private struct is responsible for keeping track of the
 * FPGA character device's minor device number (in case there are multiple streams
 * attached to the FPGA on separate files), and the hardware struct of the FPGA,
 * so that one can read/write from/to the FPGA's BAR-mapped memory. */
struct fpga_char_private_data {
        u8 minor_device_number;
        struct fpga_device *fpga_hw;
};

static struct class *fpga_dev_class;
static int major_device_number;

/* Keep track of which FPGA device this device file is connected to. */
static struct fpga_device *fpga_dev;

static int fpga_uevent(struct device *dev, struct kobj_uevent_env *env)
{
        add_uevent_var(env, "DEVMODE=%#o", 0666);

        return 0;
}

int create_char_devs(struct fpga_device *fpga)
{
        int error;
        dev_t char_dev;

        printk(KERN_DEBUG "fpga_char: creating the interactive character devices\n");

        /* Allocate a major device and minor numbers for this module. */
        error = alloc_chrdev_region(&char_dev, 0, MAX_MINOR_DEVICES, MODULE_NAME);

        major_device_number = MAJOR(char_dev);
        printk(KERN_INFO "fpga_char: Major Device Number: %d", major_device_number);

        fpga_dev_class = class_create(THIS_MODULE, "PCIe FPGA Char Class");
        fpga_dev_class->dev_uevent = fpga_uevent;

        // Initialize c-dev with these possible file operations.
        cdev_init(&fpga_dev_data.cdev, &fops);
        fpga_dev_data.cdev.owner = THIS_MODULE;
        /* Add char device to system. Use MKDEV to create a new dev_t integer
         * with the device's corresponding minor device number. In the case of a
         * single minor device, it is the same as using the dev_t directly. */
        cdev_add(&fpga_dev_data.cdev, MKDEV(major_device_number,
                                            MAX_MINOR_DEVICES - 1), 1);
        // Create the /dev entries
        fpga_dev_data.fpga_device = device_create(fpga_dev_class, NULL,
                                                  MKDEV(major_device_number, 0),
                                                  NULL, "virtine_fpga");
        // Keep track of the FPGA that just created the character device(s)
        fpga_dev = fpga;
        return 0;
}

int destroy_char_devs(void)
{
        printk(KERN_DEBUG "fpga_char: Destroying interactive character devices\n");

        // Destroy the major:minor device
        device_destroy(fpga_dev_class, MKDEV(major_device_number, 0));

        class_unregister(fpga_dev_class);
        class_destroy(fpga_dev_class);

        /* Unregister ALL devices (by unregistering the character device memory
         * region) under major device by using MINORMASK. */
        unregister_chrdev_region(MKDEV(major_device_number, 0), MINORMASK);

        return 0;
}

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

static long fpga_char_ioctl(struct file *filep, unsigned int i, unsigned long j)
{
        return 0;
}
