#include "modinfo.h"
#include "pcie_char.h"

// TODO: Change these values to their real ones.
#define VENDOR_ID 0x1172
#define DEVICE_ID 0xe003

static int fpga_probe(struct pci_dev *dev, const struct pci_device_id *id);
static void fpga_remove(struct pci_dev *dev);
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
        .name = "pcie_fpga-char",
        .id_table = fpga_id_tbl,
        .probe = fpga_probe,
        .remove = fpga_remove,
};

/*
 * @brief When a new PCIe device is detected by the kernel (either newly inserted
 * or at boot), the kernel will iterate over all the (struct pci_driver)::probe
 * functions. This will continue until the first probe that returns 0 for claiming
 * and owning the device by this module.
 */
static int fpga_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
        int error;
        struct fpga_device *fpga;
        int bar;
        unsigned long dev_mmio_start, dev_mmio_len;

        /* We must enable the PCI device. This wakes the device up,
         * allocates I/O and memory regions.
         * NOTE: This is the SAME as calling pci_enable_device_mem AND
         * pci_enable_device_io.
         * This is done by passing a bitmask of flags to a single backing
         * function. */
        error = pci_enable_device(dev);
        if(error) {
                // Could not enable the device. Exit.
                return error;
        }

        /* Create a bitmask of the BARs that match the provided flag configuration.
         * In this case, we are looking for BARs that are attached to memory. */
        bar = pci_select_bars(dev, IORESOURCE_MEM);

        /* Enable the PCI device, waking the device up, and and memory regions.
         * Lastly, enables the device's memory for use. If we wanted to use a BAR
         * that was mapped to any other type of device, then we would need to
         * use a different function.
         * NOTE: pci_enable_device() performs the same actions as
         * enable_device_(mem|io). */
        error = pci_enable_device_mem(dev);
        if (error) {
                return error;
        }

        /* Request and attempt to reserve/lock the memory regions and/or IO that
         * the BARs of this device map to. */
        error = pci_request_region(dev, bar, "PCIe FPGA Fpga");
        if (error) {
                pci_disable_device(dev);
                return error;
        }

        /* Get start of BAR0 memory offset, and the length of BAR0. */
        dev_mmio_start = pci_resource_start(dev, 0);
        dev_mmio_len = pci_resource_len(dev, 0);

        /* Allocate memory and initialize to zero for the driver's private
         * data from the kernel's normal pool of memory.
         * NOTE: The GFP_KERNEL flag means that the allocation is allowed to
         * sleep. */
        fpga = kzalloc(sizeof(struct fpga_device), GFP_KERNEL);
        if(!fpga) {
                release_device(dev);
                return -ENOMEM;
        }

        /* Bring the memory that the BAR points to into the CPU for use, and make
         * available as a pointer. */
        fpga->dev_mem = ioremap(dev_mmio_start, dev_mmio_len);

        /* Defined in pci.h. Adds pointer to private struct to the DEVICE
         * struct that backs all other (sub)types device structs. */
        pci_set_drvdata(dev, fpga);

        // NOTE: Likely need to set up DMA. pci_set_dma_mask()

        /* Read device configuration information from the config registers,
         * which is almost always safe to do. */
        pci_read_config_word(dev, PCI_VENDOR_ID, &(fpga->vendor_id));
        pci_read_config_word(dev, PCI_DEVICE_ID, &(fpga->device_id));
        printk(KERN_INFO "pci_char: Vendor: 0x%X. Device: 0x%X\n",
               fpga->vendor_id, fpga->device_id);

        create_char_devs(fpga);

        return 0;
};

/* This function is called whenever a PCIe device being handled by this driver
 * is removed (or lost) from this system.
 */
static void fpga_remove(struct pci_dev *dev)
{
        struct fpga_device *fpga = pci_get_drvdata(dev);

        destroy_char_devs();

        if(fpga) {
                kfree(fpga);
        }

        release_device(dev);
}

void release_device(struct pci_dev *dev)
{
        /* Free memory region */
        pci_release_region(dev, pci_select_bars(dev, IORESOURCE_MEM));
        /* Disable the device. */
        pci_disable_device(dev);
}

static int __init fpga_char_init(void)
{
        printk(KERN_INFO "pci_char: FPGA starting\n");

        /* Register the fpga_driver struct with the kernel fields that handle
         * this. The function returns a negative value on errors. */
        return pci_register_driver(&fpga_driver);
}

static void __exit fpga_char_exit(void)
{
        printk(KERN_INFO "pci_char: FPGA exiting\n");
        pci_unregister_driver(&fpga_driver);
}

module_init(fpga_char_init);
module_exit(fpga_char_exit);
