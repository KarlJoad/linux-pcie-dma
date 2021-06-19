#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>

#define MODULE_NAME "PCIe FPGA Echo"

// TODO: Change these values to their real ones.
#define VENDOR_ID 0x0000
#define DEVICE_ID 0x0000

static int echo_probe(struct pci_dev *dev, const struct pci_device_id *id);
static void echo_remove(struct pci_dev *dev);

/* This macro is used to create a struct pci_device_id that matches a
 * specific device.  The subvendor and subdevice fields will be set to
 * PCI_ANY_ID. */
static const struct pci_device_id fpga_id_tbl[] = {
        { PCI_DEVICE(VENDOR_ID, DEVICE_ID) },
        { }
};
MODULE_DEVICE_TABLE(pci, fpga_id_tbl);

static struct pci_driver fpga_driver = {
        .name = "PCIe FPGA Echo Driver",
        .id_table = fpga_id_tbl,
        .probe = echo_probe,
        .remove = echo_remove,
};


static struct pci_dev *fpga;

/*
 * @brief When a new PCIe device is detected by the kernel (either newly inserted
 * or at boot), the kernel will iterate over all the (struct pci_driver)::probe
 * functions. This will continue until the first probe that returns 0 for claiming
 * and owning the device by this module.
 */
static int echo_probe(struct pci_dev *dev, const struct pci_device_id *id) {
        return -2; // TODO: Switch this out with a more appropriate value.
};

/* This function is called whenever a PCIe device being handled by this driver
 * is removed (or lost) from this system.
 */
static void echo_remove(struct pci_dev *dev) {
}

static int __init echo_init(void) {
        printk(KERN_INFO "PCIe FPGA Echo starting\n");

        /* Register the fpga_driver struct with the kernel fields that handle
         * this. The function returns a negative value on errors. */
        // NOTE: Optimization: return pci_register_driver(&fpga_driver);
        if(pci_register_driver(&fpga_driver) < 0) {
                return -1;
        }

        return 0;
}

static void __exit echo_exit(void) {
        printk(KERN_INFO "PCIe FPGA Echo exiting\n");
        pci_unregister_driver(&fpga_driver);
}

module_init(echo_init);
module_exit(echo_exit);
MODULE_AUTHOR("Karl Hallsby <karl@hallsby.com>");
MODULE_DESCRIPTION("Write data to PCIe FPGA and device echoes it back out.");
MODULE_LICENSE("GPL");
