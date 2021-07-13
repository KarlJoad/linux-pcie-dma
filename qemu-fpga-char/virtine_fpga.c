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

typedef struct VirtineFpgaDevice {
    PCIDevice pdev;

    /* Does NOT correspond to the memory area. This is a call-back struct
     * for when accessing this device as an MMIO device. */
    MemoryRegion mmio;

    // The actual memory region
    // Receiving Queue (RQ) Stuff
    hwaddr *rq_base_addr;
    hwaddr *rq_head_offset_reg;
    hwaddr *rq_tail_offset_reg;
    hwaddr rq_buffer[NUM_POSSIBLE_VIRTINES];

    /* Processing signals. CPU can write to the Ready Queue (RQ) if the
     * isCardProcessing boolean is not 0. When the CPU has finished
     * transferring all the physical addresses of the virtines, the CPU can
     * "ring" the doorbell, informing the card that it can begin working.
     * If the CPU wants to transfer information while isCardProcessing is 0,
     * then it must wait.
     * The CPU receives an interrupt from the card when the provided virtines
     * are cleaned up and are ready for use again. */
    bool doorbell; // 1 to inform card that it can begin
    bool is_card_processing; // 0 if processing, anything else if not

    // Completed Queue (CQ) Stuff
    hwaddr *cq_base_addr;
    hwaddr *cq_head_offset_reg;
    hwaddr *cq_tail_offset_reg;
    hwaddr cq_buffer[NUM_POSSIBLE_VIRTINES];

    uint32_t irq_status;
    // Only raise interrupt if cleaned >= batchFactor virtines
    uint32_t batch_factor; // NOTE: For development, set batchFactor = 1
} VirtineFpgaDevice;

#define TYPE_PCI_VIRTINEFPGADEVICE "virtine-fpga"
DECLARE_INSTANCE_CHECKER(VirtineFpgaDevice, VIRTINEFPGA,
                         TYPE_PCI_VIRTINEFPGADEVICE);

/* Reading is safe, so the entire device's mmio region can be read.
 * Not really returning an int. Returning a pointer typecast as an int. */
static uint64_t virtine_fpga_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    printf("Virtine FPGA: READ @ 0x%lx of size %u\n", addr, size);
    VirtineFpgaDevice *fpga = opaque;
    uint64_t val = ~0ULL; // Assume failure
    /* FIXME: When read for large buffer, address will go past end of BAR,
     * causing failure. */
    // global_buffer is only of size 100
    if(addr >= 100) {
        return val;
    }

    val = fpga->global_buffer[addr];

    /* NOTE: Problem could be because reads are automatically dereferenced
     * NOTE: Problem could be with cat /dev/virtine_fpga test. cat could be
     * CONSTANTLY reading. */
    return val;
}

static void virtine_fpga_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                                    unsigned size)
{
    printf("Virtine FPGA: WRITE %lu to 0x%lx of size %u\n", val, addr, size);
    VirtineFpgaDevice *fpga = opaque;
    switch(addr) {
    case RQ_HEAD_OFFSET_REG:
        printf("Virtine FPGA: Writing to RQ_HEAD_OFFSET_REG\n");
        fpga->global_buffer[addr] = val;
        break;
    default:
        fpga->global_buffer[addr] = val;
        break;
    }
}

static const MemoryRegionOps virtine_fpga_mmio_ops = {
    .read = virtine_fpga_mmio_read,
    .write = virtine_fpga_mmio_write,
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
    VirtineFpgaDevice *virtine_device = VIRTINEFPGA(pci_dev);
    uint8_t *pci_conf = pci_dev->config;

    pci_config_set_interrupt_pin(pci_conf, 1);
    if (msi_init(pci_dev, 0, 1, true, false, errp)) {
        return;
    }
    memory_region_init_io(&virtine_device->mmio, OBJECT(virtine_device),
                          &virtine_fpga_mmio_ops, virtine_device,
                          "virtine_fpga-mmio", 128);

    pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &virtine_device->mmio);
    printf("Allocated and registered 1MiB of MMIO space for virtine device @ hardware address 0x%lx\n", (virtine_device->mmio).addr);

    // Set RQ
    virtine_device->rq_base_addr = &virtine_device->rq_buffer[0];
    virtine_device->rq_head_offset_reg = &virtine_device->rq_buffer[0];
    virtine_device->rq_tail_offset_reg = &virtine_device->rq_buffer[0];
    // Set CQ
    virtine_device->cq_base_addr = &virtine_device->cq_buffer[0];
    virtine_device->cq_head_offset_reg = &virtine_device->cq_buffer[0];
    virtine_device->cq_tail_offset_reg = &virtine_device->cq_buffer[0];
    // Set flag registers and batch factor
    virtine_device->doorbell = false;
    virtine_device->is_card_processing = false;
    virtine_device->batch_factor = 1;

    printf("Buildroot physical address size: %lu\n", sizeof(hwaddr));
    printf("Virtine FPGA MMIO Addresses:\n");
    printf("RQ_HEAD_OFFSET_REG: 0x%lx\n", virtine_device->rq_head_offset_reg);
    printf("RQ_TAIL_OFFSET_REG: 0x%lx\n", virtine_device->rq_tail_offset_reg);
    printf("RQ_BASE_ADDR: 0x%lx\n", virtine_device->rq_base_addr);
    printf("DOORBELL_REG: 0x%x\n", virtine_device->doorbell);
    printf("IS_PROCESSING_REG: 0x%x\n", virtine_device->is_card_processing);
    printf("CQ_HEAD_OFFSET_REG: 0x%lx\n", virtine_device->cq_head_offset_reg);
    printf("CQ_TAIL_OFFSET_REG: 0x%lx\n", virtine_device->cq_tail_offset_reg);
    printf("CQ_BASE_ADDR: 0x%lx\n", virtine_device->cq_base_addr);
    printf("BATCH_FACTOR_REG: 0x%x\n", virtine_device->batch_factor);
    printf("Virtine FPGA loaded\n");
}

/* When device is unloaded
 * Can be useful for hot(un)plugging
 */
static void virtine_fpga_uninit(PCIDevice *pci_dev)
{
    printf("Unloading Virtine FPGA\n");
    VirtineFpgaDevice *virtine_device = VIRTINEFPGA(pci_dev);
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

    // TODO: Add capabilities for MSI (0x5) and/or MSI-X (0x11)
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
        .name = TYPE_PCI_VIRTINEFPGADEVICE,
        .parent = TYPE_PCI_DEVICE,
        .instance_size = sizeof(VirtineFpgaDevice),
        .class_init = virtine_fpga_class_init,
        .interfaces = interfaces,
    };

    type_register_static(&virtine_fpga_info);
}
type_init(virtine_fpga_register_types);
