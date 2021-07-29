#include "chardev.h"

static int fpga_char_open(struct inode *inode, struct file *filep);
static int fpga_char_release(struct inode *inode, struct file *filep);
static ssize_t fpga_char_read(struct file *filep, char *buffer, size_t length,
                              loff_t *offset);
static ssize_t fpga_char_write(struct file *filep, const char *buffer,
                               size_t length, loff_t *offset);
static long fpga_char_ioctl(struct file *filep, unsigned int cmd, unsigned long args);
static int fpga_char_fasync(int fd, struct file *filep, int mode);

static const struct file_operations fops = {
        .owner = THIS_MODULE,
        .open = fpga_char_open,
        .release = fpga_char_release,
        .read = fpga_char_read,
        .write = fpga_char_write,
        .unlocked_ioctl = fpga_char_ioctl,
        .fasync = fpga_char_fasync
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
        /* u32 partitioned integer. top (32-MINORBITS) are the device's major
         * number. The lower MINORBITS is the device's minor number.
         * By default, MINORBITS is #define-d to be 20. */
        dev_t char_dev;

        pr_debug("fpga_char: creating the interactive character devices\n");

        /* Allocate a major device and minor numbers for this module. */
        error = alloc_chrdev_region(&char_dev, 0, MAX_MINOR_DEVICES, MODULE_NAME);
        if(error) { // error? negative number returned
                goto could_not_alloc_chr_region;
        }

        major_device_number = MAJOR(char_dev);
        pr_info("fpga_char: Major Device Number: %d", major_device_number);

        fpga_dev_class = class_create(THIS_MODULE, "PCIe FPGA Char Class");
        if(!fpga_dev_class) { // error? ERR_PTR() returned
                goto could_not_alloc_chr_region;
        }
        fpga_dev_class->dev_uevent = fpga_uevent;

        // Initialize c-dev with these possible file operations.
        cdev_init(&fpga_dev_data.cdev, &fops);
        fpga_dev_data.cdev.owner = THIS_MODULE;
        /* Add char device to system. Use MKDEV to create a new dev_t integer
         * with the device's corresponding minor device number. In the case of a
         * single minor device, it is the same as using the dev_t directly. */
        error = cdev_add(&fpga_dev_data.cdev,
                         MKDEV(major_device_number, MAX_MINOR_DEVICES - 1), 1);
        if(error) { // error? negative number returned
                goto could_not_add_cdev;
        }

        /* Create the device and register with sysfs, also creating the entry in
         * /dev mapping to the proper major,minor number. */
        fpga_dev_data.fpga_device = device_create(fpga_dev_class, NULL,
                                                  MKDEV(major_device_number, 0),
                                                  NULL, "virtine_fpga");
        // Keep track of the FPGA that just created the character device(s)
        fpga_dev = fpga;
        return 0;

        /* If things fail, have a roll-back area to jump to with goto */
could_not_add_cdev:
        cdev_del(&fpga_dev_data.cdev);
        class_destroy(fpga_dev_class);
could_not_alloc_chr_region:
        unregister_chrdev_region(MKDEV(major_device_number, 0), MAX_MINOR_DEVICES);
        return error;
}

int destroy_char_devs(void)
{
        pr_debug("fpga_char: Destroying interactive character devices\n");

        // Destroy the major:minor device
        device_destroy(fpga_dev_class, MKDEV(major_device_number, 0));

        pr_debug("fpga_char: Deleting kernel's cdev of device\n");
        cdev_del(&fpga_dev_data.cdev);

        pr_debug("fpga_char: Unregistering and Destroying character device class\n");
        class_destroy(fpga_dev_class);

        pr_debug("fpga_char: Unregistering and destroying %d character devices with major number %d region\n", MAX_MINOR_DEVICES, major_device_number);
        unregister_chrdev_region(MKDEV(major_device_number, 0), MAX_MINOR_DEVICES);

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

        pr_info("fpga_char: Opening character device file\n");

        fpga_char_priv = kzalloc(sizeof(struct fpga_char_private_data), GFP_KERNEL);
        if(!fpga_char_priv) {
                return -ENOMEM;
        }

        fpga_char_priv->minor_device_number = iminor(inode);
        fpga_char_priv->fpga_hw = fpga_dev;
        fpga_dev->filep = filep;

        // Give the file struct access to the character device's private struct
        filep->private_data = fpga_char_priv;

        // Increment counter for the number of times this module has been opened
        try_module_get(THIS_MODULE);

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

        pr_info("fpga_char: Closing character device file\n");

        fpga_char_priv = filep->private_data;
        if(fpga_char_priv) {
                kfree(fpga_char_priv);
                fpga_char_priv = NULL;
        }

        /* Remove this filep from list of async notified filep's */
        // fpga_char_fasync(-1, filep, 0);

        // Decrement counter for the number of times this module has been closed
        module_put(THIS_MODULE);

        return 0;
}

/* When reading, we take the given file pointer and read the requested length
 * from the offset. What the buffer points back to does NOT matter for this
 * function.
 * NOTE: This CAN be dangerous! If buffer is a pointer to user memory, this
 * function will NOT behave properly.
 * NOTE: Memory pointers are unsigned long (8 bytes, 64 bits, on amd64). */
ssize_t _fpga_char_read(struct file *filep, char *buffer, size_t length, loff_t *offset)
{
        struct fpga_char_private_data *priv = filep->private_data;
        u8 __iomem *to_read_from = priv->fpga_hw->dev_mem + *offset;
        ssize_t bytes_read = 0;
        u32 clean_virtine_addr;

        pr_debug("fpga_char: OFFSET=0x%llx\n", *offset);
        if((*offset % 4) != 0) {
                return bytes_read;
        }

        pr_debug("fpga_char: Kernel buffer @ 0x%p\n", buffer);

        while(bytes_read < length) {
                pr_debug("fpga_char: Read %zd bytes so far\n", bytes_read);
                pr_debug("fpga_char: Requested read length: %zu\n", length);
                // Read from the FPGA
                clean_virtine_addr = readl(to_read_from + bytes_read);
                pr_info("fpga_char: Reading %lu bytes from 0x%p (val: 0x%x) into buffer of size %lu",
                        sizeof(clean_virtine_addr), to_read_from + bytes_read,
                        clean_virtine_addr, sizeof(buffer));
                memcpy(buffer + bytes_read, &clean_virtine_addr, sizeof(clean_virtine_addr));
                bytes_read += sizeof(clean_virtine_addr);
        }

        return bytes_read;
}


/* Read LENGTH from the specified FILEP + OFFSET to the user buffer BUFFER. */
static ssize_t fpga_char_read(struct file *filep, char __user *buffer, size_t length,
                              loff_t *offset)
{
        struct fpga_char_private_data *priv = filep->private_data;
        u8 __iomem *to_read_from = priv->fpga_hw->dev_mem + *offset;
        unsigned long clean_virtine_addr;

        ssize_t bytes_read = 0;

        bytes_read = _fpga_char_read(filep, (char *) &clean_virtine_addr, length, offset);
        pr_debug("fpga_char: Clean Virtine Addr: 0x%lx\n", clean_virtine_addr);
        if(!bytes_read) {
                return -EIO;
        }

        pr_info("fpga_char: Reading %lu bytes from 0x%p (val: 0x%lx) into USER buffer of size %lu",
                sizeof(clean_virtine_addr), to_read_from + bytes_read,
                clean_virtine_addr, sizeof(buffer));

        // Copy the value to the provided user buffer.
        if(copy_to_user(buffer, &clean_virtine_addr,
                        sizeof(clean_virtine_addr))) {
                return -EFAULT;
        }

        return bytes_read;
}

/* NOTE: When opening this file in Python 3, you MUST pass buffering=0 to open.
 * This is because this file device does not support seek operations. */
static ssize_t fpga_char_write(struct file *filep, const char __user *buffer,
                               size_t length, loff_t *offset)
{
        struct fpga_char_private_data *priv = filep->private_data;
        u8 __iomem *to_write_to = priv->fpga_hw->dev_mem + *offset;
        unsigned int dirty_virtine_addr;

        unsigned long bytes_from_user;

        ssize_t bytes_written = 0;

        pr_debug("fpga_char: OFFSET=%llu\n", *offset);
        if((*offset % 4) != 0) {
                return bytes_written;
        }

        while(bytes_written < length) {
                 bytes_from_user = copy_from_user(&dirty_virtine_addr,
                                                  buffer + bytes_written,
                                                  sizeof(dirty_virtine_addr));
                 pr_info("fpga_char: Writing %lu bytes to 0x%p (val: 0x%x) from buffer of size %lu",
                        sizeof(dirty_virtine_addr), to_write_to + bytes_written,
                        dirty_virtine_addr, sizeof(buffer));

                 iowrite32(dirty_virtine_addr, to_write_to + bytes_written);
                 bytes_written += sizeof(dirty_virtine_addr);
        }

        /* TODO: Perform a read to the config space of the FPGA to ensure the
         * write is completed before returning.
         * First Attempt: Cannot assign the struct pci_dev to a field in
         * struct fpga_device and use that with pci_read_config_word because
         * the page for the struct pci_dev is not present. */
        return bytes_written;
}

static long fpga_char_ioctl(struct file *filep, unsigned int cmd, unsigned long args)
{
        struct fpga_char_private_data *priv = filep->private_data;

        long ret;
        switch(cmd) {
        case FPGA_CHAR_MODIFY_BATCH_FACTOR:
                // args is just the integer to write to the batch factor register
                iowrite32(args, priv->fpga_hw->dev_mem + BATCH_FACTOR_REG);
                ret = 0;
                break;
        case FPGA_CHAR_GET_MAX_NUM_VIRTINES: {
                unsigned long *num_virtines = (unsigned long*) args;
                *num_virtines = ioread32(priv->fpga_hw->dev_mem + MAX_NUM_VIRTINES_REG);
                pr_debug("fpga_char: Max Num Virtines: %lu\n", *num_virtines);
                ret = 0;
                break;
        }
        case FPGA_CHAR_RING_DOORBELL:
                pr_debug("fpga_char: Ringing doorbell!\n");
                // 1 informs card it can begin processing
                iowrite32(1, priv->fpga_hw->dev_mem + DOORBELL_REG);
                ret = 0;
                break;
        case FPGA_CHAR_SET_SNAPSHOT: {
                struct virtine_snapshot *snapshot = (struct virtine_snapshot*) args;
                // Write the 64-bit value snapshot size
                loff_t offset = SNAPSHOT_SIZE_REG;
                ret = fpga_char_write(filep, (char*) &snapshot->size,
                                      sizeof(snapshot->size), &offset);
                // Write 64-bit value of snapshot address
                offset = SNAPSHOT_ADDR_REG;
                ret = fpga_char_write(filep, (char*) &snapshot->addr,
                                      sizeof(snapshot->addr), &offset);
                // TODO: Combine the return values of both writes.
                break;
        }
        default:
                ret = -ENOTTY;
        }
        return ret;
}

// TODO: This seems to be called multiple times with info to disconnect fasync
static int fpga_char_fasync(int fd, struct file *filep, int mode)
{
        struct fpga_char_private_data *priv = filep->private_data;

        pr_debug("fpga_char: Making file reads asynchronous\n");
        pr_debug("fpga_char: fd=%d, with mode=%d\n", fd, mode);

        return fasync_helper(fd, filep, mode, &priv->async_queue);
}
