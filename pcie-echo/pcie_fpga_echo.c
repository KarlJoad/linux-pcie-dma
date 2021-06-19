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
        return pci_register_driver(&fpga_driver);
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
