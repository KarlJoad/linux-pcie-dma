#include "qemu/osdep.h"
#include "hw/hw.h"
#include "hw/pci/pci.h"
#include "qemu/event_notifier.h"

struct virtine_fpga_device {
        PCIDevice parent_device;

        /* MMIO-memory region, will mirror BAR */
        MemoryRegion mmio;

        /* Given an IRQ, for alerting CPU of clean virtines */
        qemu_irq irq;
        int threw_irq;

        /* ID of the device */
        int device_id;
};

/* When device is loaded */
static void virtine_fpga_realize(PCIDevice *pci_dev, Error **errp)
{
    /* /\* realize the internal state of the device *\/ */
    /* PCIHelloDevState *d = PCI_HELLO_DEV(pci_dev); */
    /* printf("d=%lu\n", (unsigned long) &d); */
    /* d->dma_size = 0x1ffff * sizeof(char); */
    /* d->dma_buf = malloc(d->dma_size); */
    /* d->id = 0x1337; */
    /* d->threw_irq = 0; */
    /* uint8_t *pci_conf; */

    /* /\* create the memory region representing the MMIO and PIO  */
    /*  * of the device */
    /*  *\/ */
    /* hello_io_setup(d); */
    /* /\* */
    /*  * See linux device driver (Edition 3) for the definition of a bar */
    /*  * in the PCI bus. */
    /*  *\/ */
    /* pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_IO, &d->io); */
    /* pci_register_bar(pci_dev, 1, PCI_BASE_ADDRESS_SPACE_MEMORY, &d->mmio); */

    /* pci_conf = pci_dev->config; */
    /* /\* also in ldd, a pci device has 4 pin for interrupt */
    /*  * here we use pin B. */
    /*  *\/ */
    /* pci_conf[PCI_INTERRUPT_PIN] = 0x02;  */

    /* /\* this device support interrupt *\/ */
    /* //d->irq = pci_allocate_irq(pci_dev); */

    printf("Virtine FPGA loaded\n");
}

/* When device is unloaded
 * Can be useful for hot(un)plugging
 */
static void virtine_fpga_uninit(PCIDevice *dev)
{
    /* PCIHelloDevState *d = (PCIHelloDevState *) dev; */
    /* free(d->dma_buf); */
    printf("Unloading Virtine FPGA\n");
}

static void virtine_fpga_reset(DeviceState *dev)
{
    printf("Reset Virtine FPGA\n");
}

static void virtine_fpga_class_init(ObjectClass *klass, void *data)
{
        printf("Initializing Virtine FPGA Class object\n");
        DeviceClass *dc = DEVICE_CLASS(klass);
        PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

        k->realize = virtine_fpga_realize;
        k->exit = virtine_fpga_uninit;
        /* "Create" the vendor/device IDs of our emulated device */
        k->vendor_id = 0x1337;
        k->device_id = 0x0001;
        k->revision  = 0x00;
        k->class_id = PCI_CLASS_OTHERS;
        set_bit(DEVICE_CATEGORY_MISC, dc->categories);

        dc->desc = "Virtine FPGA";

        /* qemu user things */
        // dc->props = virtine_fpga_properties;
        dc->reset = virtine_fpga_reset;
}

static void virtine_fpga_register_types(void)
{
        // Array of interfaces this device implements
        static InterfaceInfo interfaces[] = {
                { INTERFACE_CONVENTIONAL_PCI_DEVICE },
                { },
        };
        static const TypeInfo virtine_fpga_info = {
                .name = "virtine-fpga",
                .parent = TYPE_PCI_DEVICE,
                .instance_size = sizeof(struct virtine_fpga_device),
                .class_init = virtine_fpga_class_init,
                .interfaces = interfaces,
        };

        type_register_static(&virtine_fpga_info);
}
type_init(virtine_fpga_register_types);
