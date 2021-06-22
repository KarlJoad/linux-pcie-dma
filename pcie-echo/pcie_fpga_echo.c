#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>

#define MODULE_NAME "PCIe FPGA Echo"

// TODO: Change these values to their real ones.
#define VENDOR_ID 0x1172
#define DEVICE_ID 0xe003

int run_test(struct pci_dev *dev);

static int echo_probe(struct pci_dev *dev, const struct pci_device_id *id);
static void echo_remove(struct pci_dev *dev);
void release_device(struct pci_dev *pdev);

/* This macro is used to create a struct pci_device_id that matches a
 * specific device.  The subvendor and subdevice fields will be set to
 * PCI_ANY_ID. */
static const struct pci_device_id fpga_id_tbl[] = {
        { PCI_DEVICE(VENDOR_ID, DEVICE_ID) },
        { 0, }
};
MODULE_DEVICE_TABLE(pci, fpga_id_tbl); // Add device IDs to kernel's internal table

/* This struct is added to the list of structs the kernel holds, which is iterated
 * over any time a PCIe device is detected to have been connected to the machine.
 * The probe and remove function pointers allow the kernel to initialize and
 * remove the device from the kernel space in a reactive style.
 * The ID table is what maps a physical device to this struct, by means of the
 * device's vendor and device IDs.
 * The name is a way to distinguish this driver from all the others running. */
static struct pci_driver fpga_driver = {
        .name = "pcie_fpga_echo",
        .id_table = fpga_id_tbl,
        .probe = echo_probe,
        .remove = echo_remove,
};

/* This is a "private" struct, meaning the kernel does not provide or interact
 * with this struct in any way. This is supposed to be a software-side definition
 * of the required components that the driver/module can/should use to complete
 * its task. */
struct fpga_echo_device {
        u16 vendor_id;
        u16 device_id;
        u8 __iomem *dev_mem; // Pointer to mmap-ed device BAR in host's memory.
};

/*
 * @brief When a new PCIe device is detected by the kernel (either newly inserted
 * or at boot), the kernel will iterate over all the (struct pci_driver)::probe
 * functions. This will continue until the first probe that returns 0 for claiming
 * and owning the device by this module.
 */
static int echo_probe(struct pci_dev *dev, const struct pci_device_id *id) {
        int error;
        struct fpga_echo_device *fpga;
        int bar;
        unsigned long dev_mmio_start, dev_mmio_len;

        /* We must enable the PCI device. This wakes the device up,
         * allocates I/O and memory regions, and allocates an IRQ. */
        error = pci_enable_device(dev);
        if(error) {
                // Could not enable the device. Exit.
                return error;
        }

        /* Request IO BAR */
        bar = pci_select_bars(dev, IORESOURCE_MEM);

        /* Enable device memory */
        error = pci_enable_device_mem(dev);
        if (error) {
                return error;
        }

        /* Request memory region for the BAR */
        error = pci_request_region(dev, bar, "PCIe FPGA Echo");
        if (error) {
                pci_disable_device(dev);
                return error;
        }

        /* Get start and stop memory offsets */
        dev_mmio_start = pci_resource_start(dev, 0);
        dev_mmio_len = pci_resource_len(dev, 0);

        /* Allocate memory and initialize to zero for the driver's private
         * data from the kernel's normal pool of memory.
         * NOTE: The GFP_KERNEL flag means that the allocation is allowed to
         * sleep. */
        fpga = kzalloc(sizeof(struct fpga_echo_device), GFP_KERNEL);
        if(!fpga) {
                release_device(dev);
                return -ENOMEM;
        }

        /* Remap BAR to the local pointer */
        fpga->dev_mem = ioremap(dev_mmio_start, dev_mmio_len);

        /* Defined in pci.h. Adds pointer to private struct to the DEVICE
         * struct that backs all other (sub)types device structs. */
        pci_set_drvdata(dev, fpga);

        /* Read device configuration information from the config registers,
         * which is almost always safe to do. */
        pci_read_config_word(dev, PCI_VENDOR_ID, &(fpga->vendor_id));
        pci_read_config_word(dev, PCI_DEVICE_ID, &(fpga->device_id));
        printk(KERN_INFO "Device vendor: 0x%X. Device: 0x%X\n",
               fpga->vendor_id, fpga->device_id);

        /* As this is a testing module, we call the function to test if the device
         * was set up and works properly at the end of the probe function. */
        return run_test(dev);
};

/* This function is called whenever a PCIe device being handled by this driver
 * is removed (or lost) from this system.
 */
static void echo_remove(struct pci_dev *dev) {
        struct fpga_echo_device *fpga = pci_get_drvdata(dev);
        if(fpga) {
                kfree(fpga);
        }

        release_device(dev);
}

void release_device(struct pci_dev *dev) {
        /* Free memory region */
        pci_release_region(dev, pci_select_bars(dev, IORESOURCE_MEM));
        /* Disable the device. */
        pci_disable_device(dev);
}

static int __init echo_init(void) {
        printk(KERN_INFO "PCIe FPGA Echo starting\n");

        /* Register the fpga_driver struct with the kernel fields that handle
         * this. The function returns a negative value on errors. */
        return pci_register_driver(&fpga_driver);
}

static void __exit echo_exit(void) {
        printk(KERN_INFO "PCIe FPGA Echo exiting\n");
        pci_unregister_driver(&fpga_driver);
}

void write_sample_data(struct pci_dev *dev, unsigned int data) {
        struct fpga_echo_device *fpga = (struct fpga_echo_device *) pci_get_drvdata(dev);
        if(!fpga) {
                return;
        }

        /* Write 32 bits of data to the device's memory. */
        iowrite32(data, fpga->dev_mem);
}

void read_sample_data(struct pci_dev *dev, unsigned int *data) {
        struct fpga_echo_device *fpga = (struct fpga_echo_device *) pci_get_drvdata(dev);
        if(!fpga) {
                return;
        }

        /* Read 32 bits of data from device's memory. */
        *data = ioread32(fpga->dev_mem);
        return;
}

/* Runs a simple echo test. Returns zero on test success. Returns non-zero on
 * test failure. */
int run_test(struct pci_dev *dev) {
        const unsigned int test_data = 0xDEADBEEF; // 32 bits of data.
        unsigned int data_read;
        write_sample_data(dev, test_data);
        /* 0 is false, anything else is true. By default, equality comparisons
         * will return 1 if the two things are equal (==). */
        read_sample_data(dev, &data_read);
        return !(data_read == test_data);
}

module_init(echo_init);
module_exit(echo_exit);
MODULE_AUTHOR("Karl Hallsby <karl@hallsby.com>");
MODULE_DESCRIPTION("Write data to PCIe FPGA and device echoes it back out.");
MODULE_LICENSE("GPL");
