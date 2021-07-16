#include "modinfo.h"
#include "fpga_char_main.h"
#include "chardev.h"

// TODO: Change these values to their real ones.
#define VENDOR_ID 0x1172
#define DEVICE_ID 0xe003

static int fpga_probe(struct pci_dev *dev, const struct pci_device_id *id);
static void fpga_remove(struct pci_dev *dev);
static irqreturn_t fetch_clean_virtines(int irq, void *cookie);

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
        fpga->pdev = dev;

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
        if(error) { // error? non-zero returned
                goto could_not_enable_device;
        }

        /* Request and attempt to reserve/lock the memory regions and/or IO that
         * the BARs of this device map to. */
        dev_dbg(&dev->dev, "Requesting PCI device's memory regions\n");
        error = pci_request_region(dev, bar, DEVICE_NAME);
        if(error) { // error? -EBUSY returned.
                goto could_not_request_region;
        }

        /* NOTE: To allow MSI/MSI-X to work, DMA MUST also be enabled! */
        pci_set_dma_mask(dev, DMA_BIT_MASK(32)); // Give DMA a 32-bit mask.
        pci_set_master(dev); // Register this device as the master in the DMA request.
        /* Allocate MSI and/or MSI-X IRQ vectors. */
        /* params: device, minimum vectors, max vectors, type of interrupt flags */
        dev_info(&dev->dev, "Allocating MSI/MSI-X IRQs\n");
        error = pci_alloc_irq_vectors(dev, 1, NUM_IRQ_VECTORS, PCI_IRQ_MSI | PCI_IRQ_MSIX);
        if(error < NUM_IRQ_VECTORS) { // error? -1 or less than num IRQ vecs requested
                dev_err(&dev->dev, "Could not allocate MSI/MSI-X IRQs\n");
                goto could_not_request_region;
        }
        /* Get Linux IRQ num for THIS device's IRQ num with index 0.
         * Linux IRQ num is used for requesting IRQ callbacks. */
        error = pci_irq_vector(dev, 0); // Use error here for simplicity
        dev_dbg(&dev->dev, "MSI/MSI-X IRQ is: %d\n", error);
        /* Assign callback function to grabbed Linux IRQ */
        dev_info(&dev->dev, "Assigning callback to MSI/MSI-X IRQ\n");
        error = request_irq(dev->irq, fetch_clean_virtines, error,
                            "fpga_char-clean_virtine_IRQ", (void *) fpga);
        if(error < 0) {
                dev_err(&dev->dev, "Could not assign callback to MSI IRQ\n");
                goto could_not_alloc_irq_vectors;
        }

        /* Get start of BAR0 memory offset, and the length of BAR0. */
        dev_mmio_start = pci_resource_start(dev, 0);
        dev_mmio_len = pci_resource_len(dev, 0);
        /* Safe to do if no BARs selected, start will return 0 in that case. */
        dev_dbg(&dev->dev, "dev_mmio_start=0x%lx and dev_mmio_len=%lu\n",
                dev_mmio_start, dev_mmio_len);
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
        dev_err(&dev->dev, "Releasing PCI device's BARs\n");
        pci_release_region(dev, pci_select_bars(dev, IORESOURCE_MEM));
could_not_alloc_irq_vectors:
        dev_err(&dev->dev, "Removing IRQ handlers\n");
        free_irq(dev->irq, (void *) fpga);
        pci_free_irq_vectors(dev);
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

        /* Release the allocated address space from the kernel used by the FPGA */
        iounmap(fpga->dev_mem);
        /* Free the FPGA private information struct */
        if(fpga) {
                kfree(fpga);
        }

        /* Remove the callback from the main IRQ mapping */
        free_irq(dev->irq, (void *) fpga);
        /* Free the MSI/MSI-X interrupts that were allocated */
        pci_free_irq_vectors(dev);
        /* Free memory region */
        pci_release_region(dev, pci_select_bars(dev, IORESOURCE_MEM));
        /* Disable the device. */
        pci_disable_device(dev);
}

/* An IRQ handler function for fetching the clean virtines from the coprocessor
 * when it raises an interrupt on its MSI lines. */
static irqreturn_t fetch_clean_virtines(int irq, void *cookie)
{
        struct fpga_device *fpga = (struct fpga_device *) cookie;
        irqreturn_t ret;

        dev_dbg(&fpga->pdev->dev, "IRQ %d: Clean Virtines! Fetching\n", irq);
        fpga->batch_factor = 1; // TODO: Read from FPGA for batch factor.
        /* To fetch all virtines, read from CQ_HEAD_OFFSET until reading from
         * CQ stops returning useful stuff. CQ_HEAD_OFFSET will not change, but
         * the pointer it redirects to will iterate forwards through the array
         * that stores virtine hwaddrs.
         *
         * TODO: Decide how to terminate reading of CQ from FPGA.
         * When reading from a ring queue, can start returning NULL when HEAD
         * catches up to TAIL. NULL is an invalid hwaddr anyways. */

        return IRQ_HANDLED;
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
