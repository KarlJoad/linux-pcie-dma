#include "qemu/osdep.h"
#include "qemu/units.h" // Symbol definition for units
#include "hw/hw.h" // Creating Hardware
#include "hw/pci/pci.h" // Creating PCI devices
#include "hw/pci/msi.h" // MSI interrupts
#include "qemu/event_notifier.h"

/* This struct completely defines what the emulated device should have in
 * terms of hardware and signals.
 * MMIO DESIGN:
 * 0x8                               0x0
 * +-----------------------------------+
 * |    Ready Queue (RQ) Head offset   |
 * +-----------------------------------+
 * |    Ready Queue (RQ) Tail offset   |
 * +-----------------------------------+
 * |           RQ Virtine 1            |
 * +-----------------------------------+
 * |           RQ Virtine 2            |
 * +-----------------------------------+
 * |               .....               |
 * +-----------------------------------+
 * |              Doorbell             |
 * +-----------------------------------+
 * |          isCardProcessing         |
 * +-----------------------------------+
 * |  Complete Queue (CQ) Head offset  |
 * +-----------------------------------+
 * |  Complete Queue (CQ) Tail offset  |
 * +-----------------------------------+
 * |           CQ Virtine 1            |
 * +-----------------------------------+
 * |               .....               |
 * +-----------------------------------+
 * |            Batch Factor           |
 * +-----------------------------------+ */

#define MMIO_BASE_ADDR 0x0
#define NUM_POSSIBLE_VIRTINES 100

#define RQ_HEAD_OFFSET_REG MMIO_BASE_ADDR
#define RQ_TAIL_OFFSET_REG RQ_HEAD_OFFSET_REG + sizeof(unsigned long)
#define RQ_BASE_ADDR RQ_TAIL_OFFSET_REG + sizeof(unsigned long)
#define DOORBELL_REG RQ_BASE_ADDR + (NUM_POSSIBLE_VIRTINES * sizeof(unsigned long))
#define IS_PROCESSING_REG DOORBELL_REG + sizeof(unsigned long)
#define CQ_HEAD_OFFSET_REG IS_PROCESSING_REG + sizeof(unsigned long)
#define CQ_TAIL_OFFSET_REG CQ_HEAD_OFFSET_REG + sizeof(unsigned long)
#define CQ_BASE_ADDR CQ_TAIL_OFFSET_REG + sizeof(unsigned long)
#define BATCH_FACTOR_REG CQ_BASE_ADDR + (NUM_POSSIBLE_VIRTINES * sizeof(unsigned long))

#define PCI_CLASS_COPROCESSOR 0x12

struct virtine_fpga_device {
        PCIDevice pdev;
        MemoryRegion mmio;

        /* Processing signals. CPU can write to the Ready Queue (RQ) if the
         * isCardProcessing boolean is not 0. When the CPU has finished
         * transferring all the physical addresses of the virtines, the CPU can
         * "ring" the doorbell, informing the card that it can begin working.
         * If the CPU wants to transfer information while isCardProcessing is 0,
         * then it must wait.
         * The CPU receives an interrupt from the card when the provided virtines
         * are cleaned up and are ready for use again. */
        bool doorbell; // 1 to inform card that it can begin
        bool isCardProcessing; // 0 if processing, anything else if not

        uint32_t irq_status;
        // Only raise interrupt if cleaned >= batchFactor virtines
        uint32_t batchFactor; // NOTE: For development, set batchFactor = 1
};

#define TYPE_PCI_VIRTINE_FPGA_DEVICE "virtine-fpga"
typedef struct virtine_fpga_device virtine_fpga_device;
DECLARE_INSTANCE_CHECKER(virtine_fpga_device, VIRTINEFPGA,
                         TYPE_PCI_VIRTINE_FPGA_DEVICE);

static const MemoryRegionOps virtine_fpga_mmio_ops = {
        .read = NULL,
        .write = NULL,
        .endianness = DEVICE_NATIVE_ENDIAN,
        // NOTE: Values below are in BYTES!
        .valid = {
                .min_access_size = 4,
                .max_access_size = 8,
        },
        .impl = {
                .min_access_size = 4,
                .max_access_size = 8,
        },
};

/* When device is loaded */
static void virtine_fpga_realize(PCIDevice *pci_dev, Error **errp)
{
        virtine_fpga_device *virtine_device = VIRTINEFPGA(pci_dev);
        uint8_t *pci_conf = pci_dev->config;

        pci_config_set_interrupt_pin(pci_conf, 1);
        if (msi_init(pci_dev, 0, 1, true, false, errp)) {
                return;
        }
        memory_region_init_io(&virtine_device->mmio, OBJECT(virtine_device),
                              &virtine_fpga_mmio_ops, virtine_device,
                              "virtine_fpga-mmio", 1 * MiB);
        pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &virtine_device->mmio);

        printf("Buildroot physical address size: %lu\n", sizeof(unsigned long));
        printf("Virtine FPGA MMIO Addresses:\n");
        printf("RQ_HEAD_OFFSET_REG: 0x%lx\n", (unsigned long) RQ_HEAD_OFFSET_REG);
        printf("RQ_TAIL_OFFSET_REG: 0x%lx\n", RQ_TAIL_OFFSET_REG);
        printf("RQ_BASE_ADDR: 0x%lx\n", RQ_BASE_ADDR);
        printf("DOORBELL_REG: 0x%lx\n", DOORBELL_REG);
        printf("IS_PROCESSING_REG: 0x%lx\n", IS_PROCESSING_REG);
        printf("CQ_HEAD_OFFSET_REG: 0x%lx\n", CQ_HEAD_OFFSET_REG);
        printf("CQ_TAIL_OFFSET_REG: 0x%lx\n", CQ_TAIL_OFFSET_REG);
        printf("CQ_BASE_ADDR: 0x%lx\n", CQ_BASE_ADDR);
        printf("BATCH_FACTOR_REG: 0x%lx\n", BATCH_FACTOR_REG);
        printf("Virtine FPGA loaded\n");
}

/* When device is unloaded
 * Can be useful for hot(un)plugging
 */
static void virtine_fpga_uninit(PCIDevice *pci_dev)
{
    printf("Unloading Virtine FPGA\n");
    virtine_fpga_device *virtine_device = VIRTINEFPGA(pci_dev);
    msi_uninit(pci_dev);
    memory_region_unref(&virtine_device->mmio);
}

/* static void virtine_fpga_reset(DeviceState *dev) */
/* { */
/*     printf("Reset Virtine FPGA\n"); */
/* } */

static void virtine_fpga_class_init(ObjectClass *klass, void *data)
{
        printf("Initializing Virtine FPGA Class object\n");
        DeviceClass *dc = DEVICE_CLASS(klass);
        PCIDeviceClass *k = PCI_DEVICE_CLASS(klass);

        k->realize = virtine_fpga_realize;
        k->exit = virtine_fpga_uninit;
        /* "Create" the vendor/device IDs of our emulated device */
        k->vendor_id = 0x1172;
        k->device_id = 0xE003;
        k->revision  = 0x00;
        k->class_id = PCI_CLASS_COPROCESSOR;
        set_bit(DEVICE_CATEGORY_MISC, dc->categories);

        dc->desc = "Virtine FPGA";

        /* qemu user things */
        // dc->props = virtine_fpga_properties;
        // dc->reset = virtine_fpga_reset;
}

static void virtine_fpga_register_types(void)
{
        // Array of interfaces this device implements
        static InterfaceInfo interfaces[] = {
                { INTERFACE_CONVENTIONAL_PCI_DEVICE },
                { },
        };
        static const TypeInfo virtine_fpga_info = {
                .name = TYPE_PCI_VIRTINE_FPGA_DEVICE,
                .parent = TYPE_PCI_DEVICE,
                .instance_size = sizeof(struct virtine_fpga_device),
                .class_init = virtine_fpga_class_init,
                .interfaces = interfaces,
        };

        type_register_static(&virtine_fpga_info);
}
type_init(virtine_fpga_register_types);
