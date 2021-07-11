#include "modinfo.h"
#include "fpga_char_main.h"
#include "chardev.h"

// TODO: Change these values to their real ones.
#define VENDOR_ID 0x1172
#define DEVICE_ID 0xe003

static int fpga_probe(struct pci_dev *dev, const struct pci_device_id *id);
static void fpga_remove(struct pci_dev *dev);
static inline void release_device(struct pci_dev *pdev);

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
        .name = "fpga_char_main",
        .id_table = fpga_id_tbl,
        .probe = fpga_probe,
        .remove = fpga_remove,
};

/*
 * @brief When a new PCIe device is detected by the kernel (either newly inserted
 * or at boot), the kernel will iterate over all the (struct fpga_driver)::probe
 * functions. This will continue until the first probe that returns 0 for claiming
 * and owning the device by this module.
 */
static int fpga_probe(struct pci_dev *dev, const struct pci_device_id *id)
{
        int error;
        int bar;
        unsigned long dev_mmio_start, dev_mmio_len;

        /* Allocate memory and initialize to zero for the driver's private
         * data from the kernel's normal pool of memory.
         * NOTE: The GFP_KERNEL flag means that the allocation is allowed to
         * sleep. */
        struct fpga_device *fpga = kzalloc(sizeof(struct fpga_device), GFP_KERNEL);
        if(!fpga) { // error? NULL pointer returned
                return -ENOMEM;
        }

        /* We must enable the PCI device. This wakes the device up,
         * allocates I/O and memory regions.
         * NOTE: This is the SAME as calling fpga_enable_device_mem AND
         * fpga_enable_device_io.
         * This is done by passing a bitmask of flags to a single backing
         * function. */
        dev_info(&dev->dev, "Enabling Device\n");
        error = pci_enable_device(dev);
        if(error) { // error? non-zero returned
                goto could_not_enable_device;
        }

        /* Create a bitmask of the BARs that match the provided flag configuration.
         * In this case, we are looking for BARs that are attached to memory. */
        bar = pci_select_bars(dev, IORESOURCE_MEM);
        dev_dbg(&dev->dev, "Bitmask of all memory BARs 0x%x\n", bar);

        /* Enable the PCI device, waking the device up, and and memory regions.
         * Lastly, enables the device's memory for use. If we wanted to use a BAR
         * that was mapped to any other type of device, then we would need to
         * use a different function.
         * NOTE: fpga_enable_device() performs the same actions as
         * enable_device_(mem|io). */
        error = pci_enable_device_mem(dev);
        if (error) { // error? non-zero returned
                goto could_not_enable_device;
        }

        /* Request and attempt to reserve/lock the memory regions and/or IO that
         * the BARs of this device map to. */
        dev_dbg(&dev->dev, "Requesting PCI device's memory regions\n");
        error = pci_request_region(dev, bar, DEVICE_NAME);
        if (error) { // error? -EBUSY returned.
                goto could_not_request_region;
        }

        /* Get start of BAR0 memory offset, and the length of BAR0. */
        dev_mmio_start = pci_resource_start(dev, 2);
        dev_mmio_len = pci_resource_len(dev, 2);
        /* Safe to do if no BARs selected, start will return 0 in that case. */
        dev_dbg(&dev->dev, "dev_mmio_start=0x%lx and dev_mmio_len=%lu when no matching BAR present\n", dev_mmio_start, dev_mmio_len);
        if(dev_mmio_start == 0 && dev_mmio_len == 0) {
                error = -ENODEV;
                dev_err(&dev->dev, "Cannot match BARs to resource request. Exiting with %d!\n", error);
                goto ioremap_failed;
        }

        /* Bring the memory that the BAR points to into the CPU for use, and make
         * available as a pointer. */
        dev_dbg(&dev->dev, "Remapping the PCI BAR memory and marking as uncachable\n");
        fpga->dev_mem = ioremap_uc(dev_mmio_start, dev_mmio_len);
        if(!(fpga->dev_mem)) { // error? NULL pointer returned
                goto ioremap_failed;
        }

        dev_dbg(&dev->dev, "Remapped BAR 0 from 0x%lx to 0x%p\n", dev_mmio_start, fpga->dev_mem);
        // NOTE: Likely need to set up DMA. fpga_set_dma_mask()

        /* Read device configuration information from the config registers,
         * which is almost always safe to do. */
        pci_read_config_word(dev, PCI_VENDOR_ID, &(fpga->vendor_id));
        pci_read_config_word(dev, PCI_DEVICE_ID, &(fpga->device_id));
        dev_info(&dev->dev, "Vendor: 0x%X. Device: 0x%X\n",
                 fpga->vendor_id, fpga->device_id);

        error = create_char_devs(fpga);
        if(error) { // error? non-zero returned
                goto char_devs_failed;
        }

        /* Defined in pci.h. Adds pointer to private struct to the DEVICE
         * struct that backs all other (sub)types device structs. */
        pci_set_drvdata(dev, fpga);

        return 0;

char_devs_failed:
        iounmap(fpga->dev_mem);
ioremap_failed:
        dev_err(&dev->dev, "Releasing PCI device's BARs");
        pci_release_region(dev, pci_select_bars(dev, IORESOURCE_MEM));
could_not_request_region:
        dev_err(&dev->dev, "Disabling PCI device, for safety\n");
        pci_disable_device(dev);
could_not_enable_device:
        dev_crit(&dev->dev, "Not installing this driver for this device with error code: %d", error);
        return error;
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

        /* Release the allocated address space from the kernel used by the FPGA */
        iounmap(fpga->dev_mem);

        release_device(dev);
}

static inline void release_device(struct pci_dev *dev)
{
        /* Free memory region */
        pci_release_region(dev, pci_select_bars(dev, IORESOURCE_MEM));
        /* Disable the device. */
        pci_disable_device(dev);
}

static int __init fpga_char_main_init(void)
{
        pr_info("fpga_char_main: FPGA character driver starting\n");

        /* Register the fpga_driver struct with the kernel fields that handle
         * this. The function returns a negative value on errors. */
        return pci_register_driver(&fpga_driver);
}

static void __exit fpga_char_main_exit(void)
{
        pr_info("fpga_char_main: FPGA character driver exiting\n");
        pci_unregister_driver(&fpga_driver);
}

module_init(fpga_char_main_init);
module_exit(fpga_char_main_exit);
