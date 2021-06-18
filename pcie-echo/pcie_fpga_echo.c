#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>

#define MODULE_NAME "PCIe FPGA Echo"
#define DRIVER_AUTHOR "Karl Hallsby <karl@hallsby.com>"
#define DRIVER_DESCRIPTION "Write data to PCIe FPGA and device echoes it back out."

// TODO: Change these values to their real ones.
#define VENDOR_ID 0x0000
#define DEVICE_ID 0x0000

static int __init echo_init(void) {
        printk(KERN_INFO "PCIe FPGA Echo starting\n");

        /* Attempt to grab the device's struct from the kernel by using the
         * VENDOR_ID and DEVICE_ID that we defined when we built the FPGA's
         * bitstream. */
        fpga = pci_get_device(VENDOR_ID, DEVICE_ID, fpga);
        if(fpga == NULL) {
                printk("pcie_fpga_echo - FPGA is either not available or does not have the correct bitstream flashed\n");
                return -1;
        }

        if(pci_enable_device(fpga) < 0) {

                return -1;
        }

        return 0;
}

static void __exit echo_exit(void) {
        printk(KERN_INFO "PCIe FPGA Echo exiting\n");
        pci_dev_put(fpga);
}

module_init(echo_init);
module_exit(echo_exit);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_LICENSE("GPL");
