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

/* Changes the RWX bits of the /dev file created by the device_create call in
 * create_char_devs. */
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

/* Because the the corresponding device file in /dev is backed by the PCI driver
 * and is connected to an FPGA's memory, "opening" the FPGA file is tantamount
 * to allocating the memory for the FPGA character device's private struct and
 * getting that set up.
 * This is also simplified by the fact that one cannot seek within the file. */
static int fpga_char_open(struct inode *inode, struct file *filep)
{
        // NOTE: llseek is NOT supported by this device. Call appropriately.
        struct fpga_char_private_data *fpga_char_priv;

        printk(KERN_INFO "fpga_char: Opening character device file\n");

        fpga_char_priv = kzalloc(sizeof(struct fpga_char_private_data), GFP_KERNEL);
        if(!fpga_char_priv) {
                return -ENOMEM;
        }

        fpga_char_priv->minor_device_number = iminor(inode);
        fpga_char_priv->fpga_hw = fpga_dev;

        // Give the file struct access to the character device's private struct
        filep->private_data = fpga_char_priv;

        return 0;
}

/* The release function for the FPGA character device is essentially just
 * deallocating and cleaning up ANYTHING and EVERYTHING we created during the
 * open process.
 * Because this device is backed by the PCI driver, this amounts to just freeing
 * the private struct that we use to track this device. */
static int fpga_char_release(struct inode *inode, struct file *filep)
{
        struct fpga_char_private_data *fpga_char_priv;

        printk(KERN_INFO "fpga_char: Closing character device file\n");

        fpga_char_priv = filep->private_data;
        if(fpga_char_priv) {
                kfree(fpga_char_priv);
                fpga_char_priv = NULL;
        }

        return 0;
}

/* When reading, we take the memory pointer given at the front of the FPGA's BAR
 * and write it out to BUFFER.
 * NOTE: Memory pointers are unsigned long (8 bytes, 64 bits, on amd64). */
static ssize_t fpga_char_read(struct file *filep, char __user *buffer, size_t length,
                              loff_t *offset)
{
        struct fpga_char_private_data *priv = filep->private_data;
        unsigned long clean_virtine_buf; // [256]
        u8 __iomem *to_read;
        clean_virtine_buf = kzalloc(256, GFP_KERNEL);

        to_read = priv->fpga_hw->dev_mem;
        printk(KERN_DEBUG "fpga_char: Reading %lu bytes @ 0x%p into buffer of size %lu",
               length, to_read, sizeof(buffer));

        *clean_virtine_buf = readq(to_read);

        if(copy_to_user(buffer, &clean_virtine_buf, sizeof(buffer))) {
                return -EFAULT;
        }

        kfree(clean_virtine_buf);

        return length - sizeof(buffer);
}

static ssize_t fpga_char_write(struct file *filep, const char __user *buffer,
                               size_t length, loff_t *offset)
{
        // NOTE: Memory pointers are unsigned long (8 bytes, 64 bits, on amd64).
        return length;
}

static long fpga_char_ioctl(struct file *filep, unsigned int i, unsigned long j)
{
        return 0;
}
