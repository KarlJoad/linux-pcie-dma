#include "qemu/osdep.h"
#include "qemu/units.h" // Symbol definition for units
#include "hw/hw.h" // Creating Hardware
#include "hw/pci/pci.h" // Creating PCI devices
#include "hw/pci/msi.h" // MSI interrupts
#include "qemu/event_notifier.h"

/* This struct completely defines what the emulated device should have in
 * terms of hardware and signals.
 * MMIO DESIGN (Remember PCI is little-endian):
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

// Take the base address and add space required to move to next location.
#define RQ_HEAD_OFFSET_REG MMIO_BASE_ADDR
#define RQ_TAIL_OFFSET_REG RQ_HEAD_OFFSET_REG + sizeof(hwaddr)
#define RQ_BASE_ADDR RQ_TAIL_OFFSET_REG + sizeof(hwaddr)
#define DOORBELL_REG RQ_BASE_ADDR + (NUM_POSSIBLE_VIRTINES * sizeof(hwaddr))
#define IS_PROCESSING_REG DOORBELL_REG + sizeof(unsigned long)
#define CQ_HEAD_OFFSET_REG IS_PROCESSING_REG + sizeof(hwaddr)
#define CQ_TAIL_OFFSET_REG CQ_HEAD_OFFSET_REG + sizeof(hwaddr)
#define CQ_BASE_ADDR CQ_TAIL_OFFSET_REG + sizeof(hwaddr)
#define BATCH_FACTOR_REG CQ_BASE_ADDR + (NUM_POSSIBLE_VIRTINES * sizeof(hwaddr))
#define MAX_NUM_VIRTINES_REG BATCH_FACTOR_REG + sizeof(unsigned long)
#define SNAPSHOT_SIZE_REG MAX_NUM_VIRTINES_REG + sizeof(unsigned long)
#define SNAPSHOT_ADDR_REG SNAPSHOT_SIZE_REG + sizeof(unsigned long)

#define PCI_CLASS_COPROCESSOR 0x12

#define PROCESSING 0

struct virtine_ring_queue {
    hwaddr *base_addr; // Also referred to as BASE
    hwaddr *head_offset; // Also referred to as HEAD
    hwaddr *tail_offset; // Also referred to as TAIL
    hwaddr buffer[NUM_POSSIBLE_VIRTINES];
};
static inline void reset_queue_pointers(struct virtine_ring_queue *queue);
static inline hwaddr* end_of_queue(struct virtine_ring_queue *queue);
static inline bool within(hwaddr *p, struct virtine_ring_queue *queue);
static hwaddr* next_element(struct virtine_ring_queue *queue, hwaddr *p);
static hwaddr* peek_previous_element(struct virtine_ring_queue *queue, hwaddr *p) __attribute__((unused));
static hwaddr pop_head(struct virtine_ring_queue *queue);
static hwaddr* insert_tail(struct virtine_ring_queue *queue, hwaddr to_insert);

typedef struct VirtineFpgaDevice {
    PCIDevice pdev;

    /* Does NOT correspond to the memory area. This is a call-back struct
     * for when accessing this device as an MMIO device. */
    MemoryRegion mmio;

    // The actual memory region
    // Receiving Queue (RQ) Stuff
    struct virtine_ring_queue rq;

    /* Processing signals. CPU can write to the Ready Queue (RQ) if the
     * isCardProcessing boolean is not 0. When the CPU has finished
     * transferring all the physical addresses of the virtines, the CPU can
     * "ring" the doorbell, informing the card that it can begin working.
     * If the CPU wants to transfer information while isCardProcessing is 0,
     * then it must wait.
     * These must be atomic, for the co-processing thread to be safe.
     * The CPU receives an interrupt from the card when the provided virtines
     * are cleaned up and are ready for use again.
     * +----------+-----------------+----------------------------------------+
     * | Doorbell | Card Processing |                  Result                |
     * +----------+-----------------+----------------------------------------+
     * |    0     |        0        |                Undefined               |
     * |    0     |        1        |       Doorbell rung. Card processing   |
     * |    1     |        0        | Doorbell rung. Card not processing yet |
     * |    1     |        1        |   Doorbell rung. Card about to start.  |
     * +----------+-----------------+----------------------------------------+ */
    bool doorbell; // 1 to inform card that it can begin
    bool is_card_processing; // 0 if processing, anything else if not
    QemuThread processing_thread;
    QemuMutex processing_lock;
    QemuCond processing_condition;

    // Completed Queue (CQ) Stuff
    struct virtine_ring_queue cq;

    uint32_t irq_status;
    // Only raise interrupt if cleaned >= batchFactor virtines
    uint32_t batch_factor; // NOTE: For development, set batchFactor = 1
    uint32_t num_virtines_cleaned_already;

    // Store restoration snapshot
    uint64_t snapshot_size;
    hwaddr *snapshot_addr;

    // Used for cleaning up co-processor thread before QEMU device is uninit-ed
    bool stopping;
} VirtineFpgaDevice;

#define TYPE_PCI_VIRTINEFPGADEVICE "virtine-fpga"
DECLARE_INSTANCE_CHECKER(VirtineFpgaDevice, VIRTINEFPGA,
                         TYPE_PCI_VIRTINEFPGADEVICE);

/* Reading is safe, so the entire device's mmio region can be read.
 * Not really returning an int. Returning a pointer typecast as an int. */
static uint64_t virtine_fpga_mmio_read(void *opaque, hwaddr addr, unsigned size)
{
    VirtineFpgaDevice *fpga = opaque;
    uint64_t val = ~0ULL; // Assume failure

    /* When popping 64-bit hwaddrs from the ring buffers with 32-bit reads, the
     * actual reads issued by the kernel will be to HEAD address and HEAD + 4.
     * To get the entire 64-bit address, when the HEAD address is popped, the
     * returned value is stored in this static variable, so that when the next
     * 32-bit read is requested, the upper 32-bits can be returned. */
    static uint64_t split_return = 0;

    switch(addr) {
    case RQ_BASE_ADDR:
        printf("Virtine FPGA: Read from RQ_BASE_ADDR\n");
        break;
    case (RQ_BASE_ADDR + 4):
        printf("Virtine FPGA: Read from RQ_BASE_ADDR + 4\n");
        break;
    case RQ_HEAD_OFFSET_REG:
        printf("Virtine FPGA: Read from RQ_HEAD_OFFSET_REG\n");
        break;
    case (RQ_HEAD_OFFSET_REG + 4):
        printf("Virtine FPGA: Read from RQ_HEAD_OFFSET_REG + 4\n");
        break;
    case RQ_TAIL_OFFSET_REG:
        printf("Virtine FPGA: Read from RQ_TAIL_OFFSET_REG\n");
        break;
    case (RQ_TAIL_OFFSET_REG + 4):
        printf("Virtine FPGA: Read from RQ_TAIL_OFFSET_REG + 4\n");
        break;
    case CQ_BASE_ADDR:
        printf("Virtine FPGA: Read from CQ_BASE_ADDR\n");
        val = (uint64_t) *fpga->cq.base_addr;
        break;
    case (CQ_BASE_ADDR + 4):
        val = ((uint64_t) *fpga->cq.base_addr) >> 32;
        break;
    case CQ_HEAD_OFFSET_REG:
        printf("Virtine FPGA: Read from CQ_HEAD_OFFSET_REG\n");
        qemu_mutex_lock(&fpga->processing_lock);
        split_return = pop_head(&fpga->cq);
        qemu_mutex_unlock(&fpga->processing_lock);
        val = split_return;
        break;
    case (CQ_HEAD_OFFSET_REG + 4):
        val = split_return >> 32;
        split_return = 0;
        break;
    case CQ_TAIL_OFFSET_REG:
        printf("Virtine FPGA: Read from CQ_TAIL_OFFSET_REG\n");
        val = (uint64_t) *fpga->cq.tail_offset;
        break;
    case (CQ_TAIL_OFFSET_REG + 4):
        val = ((uint64_t) *fpga->cq.tail_offset) >> 32;
        break;
    case IS_PROCESSING_REG:
        printf("Virtine FPGA: Read from IS_PROCESSING_REG\n");
        val = qatomic_read(&fpga->is_card_processing);
        break;
    case BATCH_FACTOR_REG:
        printf("Virtine FPGA: Read from BATCH_FACTOR_REG with value\n");
        val = fpga->batch_factor;
        break;
    case DOORBELL_REG:
        printf("Virtine FPGA: Reading Doorbell\n");
        val = qatomic_read(&fpga->doorbell);
        break;
    case MAX_NUM_VIRTINES_REG:
        printf("Virtine FPGA: Returning maximum number of virtines that can be handled\n");
        val = NUM_POSSIBLE_VIRTINES;
        break;
    case SNAPSHOT_SIZE_REG:
        printf("Virtine FPGA: Returning size of virtine snapshot\n");
        val = fpga->snapshot_size;
        break;
    case SNAPSHOT_ADDR_REG:
        printf("Virtine FPGA: Returning hwaddr of virtine snapshot\n");
        val = (uint64_t) fpga->snapshot_addr;
        break;
    case (SNAPSHOT_ADDR_REG + 4):
        val = (uint64_t) fpga->snapshot_addr >> 32;
        break;
    default:
        printf("Unknown read address. Failing!\n");
        break;
    }

    printf("Virtine FPGA: READ %lu (0x%lx) from 0x%lx of size %u\n", val, val, addr, size);
    return val;
}

/* If we want to provide dynamic reconfiguration, the write function requires the
 * ability to write to most of the memory space. Thus, some protections should
 * be implemented in the kernel driver, rather than the QEMU device. */
static void virtine_fpga_mmio_write(void *opaque, hwaddr addr, uint64_t val,
                                    unsigned size)
{
    printf("Virtine FPGA: WRITE %lu (0x%lx) to 0x%lx of size %u\n", val, val, addr, size);
    VirtineFpgaDevice *fpga = opaque;

    static hwaddr split_write = 0;

    /* Each case corresponds to writing to a register. To prevent the writing to
     * a range of memory (the CQ virtines), we fall through to default and then
     * figure out which range it is, providing access as needed. */
    switch(addr) {
    case RQ_BASE_ADDR:
        printf("Virtine FPGA: Attempted write to RQ_BASE_ADDR! Fail!\n");
        break;
    case RQ_HEAD_OFFSET_REG:
        printf("Virtine FPGA: Attempted write to RQ_HEAD_OFFSET_REG! Fail!\n");
        break;
    case RQ_TAIL_OFFSET_REG:
        printf("Virtine FPGA: Write to RQ_TAIL_OFFSET_REG! Wait for next 32 bits!\n");
        split_write = val;
        break;
    case (RQ_TAIL_OFFSET_REG + 4):
        printf("Virtine FPGA: Got next 32 bits of virtine addr. Write to queue!\n");
        val = val << 32;
        split_write = val | split_write;
        printf("Virtine FPGA: Val to write to RQ @ %p: 0x%lx\n", fpga->rq.tail_offset,
               split_write);
        qemu_mutex_lock(&fpga->processing_lock);
        insert_tail(&fpga->rq, split_write);
        qemu_mutex_unlock(&fpga->processing_lock);
        break;
    case CQ_BASE_ADDR:
        printf("Virtine FPGA: Attempted write to CQ_BASE_ADDR! Fail!\n");
        break;
    case CQ_HEAD_OFFSET_REG:
        printf("Virtine FPGA: Attempted write to CQ_HEAD_OFFSET_REG! Fail!\n");
        break;
    case CQ_TAIL_OFFSET_REG:
        printf("Virtine FPGA: Attempted write to CQ_TAIL_OFFSET_REG! Fail!\n");
        break;
    case IS_PROCESSING_REG:
        printf("Virtine FPGA: Attempt to write to IS_PROCESSING_REG. Failing.\n");
        break;
    case BATCH_FACTOR_REG:
        printf("Virtine FPGA: Write to BATCH_FACTOR_REG with value %lu\n", val);
        fpga->batch_factor = val;
        break;
    case DOORBELL_REG:
        printf("Virtine FPGA: Ringing Doorbell to start clean-up\n");
        /* Can set to PROCESSING as many times as you want. Will be set to 0
         * inside the processing_lock exclusion zone in virtine_fpga_virtine_cleanup. */
        qatomic_set(&fpga->doorbell, true);
        qemu_cond_signal(&fpga->processing_condition);
        break;
    case MAX_NUM_VIRTINES_REG:
        printf("Virtine FPGA: Writing max num virtines that can be handled. Failing.\n");
        break;
    case SNAPSHOT_SIZE_REG:
        printf("Virtine FPGA: Setting size of virtine snapshot\n");
        fpga->snapshot_size = val;
        break;
    case SNAPSHOT_ADDR_REG:
        printf("Virtine FPGA: Storing hwaddr of virtine snapshot\n");
        fpga->snapshot_addr = (hwaddr *) val;
        break;
    case (SNAPSHOT_ADDR_REG + 4): {
        // Bit shift upper 4 bytes to correct position and bitwise OR old number together
        uint64_t temp = (uint64_t) fpga->snapshot_addr;
        fpga->snapshot_addr = (hwaddr *) ((val << 32) | temp);
        break;
    }
    default:
        if((addr >= CQ_BASE_ADDR) &&
           (addr < CQ_BASE_ADDR + NUM_POSSIBLE_VIRTINES)) {
            printf("Virtine FPGA: Attempt to write to Clean Virtine Queue. Failing.\n");
        }
        else {
            printf("Unknown write address. Failing!\n");
        }

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

static void* virtine_fpga_virtine_cleanup(void *opaque)
{
    VirtineFpgaDevice *fpga = opaque;
    printf("Virtine FPGA: Starting virtine cleanup co-processor thread!\n");
    while(true) {
        printf("Virtine FPGA: Starting clean-up waiting busy-loop again!\n");
        qemu_mutex_lock(&fpga->processing_lock);

        // If doorbell has not been rung and fpga is not stopping, wait.
        while((qatomic_read(&fpga->doorbell) == false) && !fpga->stopping) {
            qemu_cond_wait(&fpga->processing_condition, &fpga->processing_lock);
        }

        /* At this point, either the doorbell has been rung, which means we
         * begin processing, or the the FPGA is stopping (QEMU shutting down)
         * in which case we need to let this thread re-join the main process */
        if(fpga->stopping) {
            qemu_mutex_unlock(&fpga->processing_lock);
            break;
        }

        // Signal that FPGA is doing work.
        qatomic_set(&fpga->is_card_processing, true);
        qatomic_set(&fpga->doorbell, false);

        printf("Virtine FPGA: Cleaning up virtines!!\n");

        hwaddr *virtine_to_clean = (hwaddr *) pop_head(&fpga->rq);
        while(virtine_to_clean) {
            printf("Virtine FPGA: Cleaning virtine @ %p\n", virtine_to_clean);

            // Copy the snapshot over the old virtine's memory, cleaning the virtine
            memcpy(virtine_to_clean, fpga->snapshot_addr, fpga->snapshot_size);
            // Update number of virtines cleaned
            fpga->num_virtines_cleaned_already += 1;

            // Move the clean virtine to clean queue
            insert_tail(&fpga->cq, (hwaddr) virtine_to_clean);
            /* TODO: If insert_tail returns == fpga->cq.tail_offset, then raise
             * interrupt and wait for some of them to be copied out before writing
             * this newly cleaned virtine to queue. */

            printf("Virtine FPGA: RQ Base: %p\n", fpga->rq.base_addr);
            printf("Virtine FPGA: Val @ RQ Base: %lx\n", *fpga->rq.base_addr);
            printf("Virtine FPGA: RQ HEAD: %p\n", fpga->rq.head_offset);
            printf("Virtine FPGA: Val @ RQ HEAD: %lx\n", *fpga->rq.head_offset);
            printf("Virtine FPGA: CQ HEAD: %p\n", fpga->cq.head_offset);
            printf("Virtine FPGA: Val @ CQ HEAD: 0x%lx\n", *fpga->cq.head_offset);

            qatomic_set(&fpga->is_card_processing, false);

            // Raise an interrupt to CPU that computation completed
            if(fpga->num_virtines_cleaned_already >= fpga->batch_factor) {
                qemu_mutex_lock_iothread();
                printf("Virtine FPGA: Sending MSI notification!\n");
                msi_notify(&fpga->pdev, 0); // Raise IRQ on device's MSI vector 0
                qemu_mutex_unlock_iothread();
                // Reset number of virtines cleaned after raising interrupt
                /* NOTE: May need to reset number of virtines cleaned already
                 * on the kernel's IRQ handler and have this part of the process
                 * wait until the CPU finishes copying already cleaned virtines
                 * out of the FPGA. */
                fpga->num_virtines_cleaned_already = 0;
            }
            virtine_to_clean = (hwaddr *) pop_head(&fpga->rq);
        }
        qemu_mutex_unlock(&fpga->processing_lock);
        // Repeat this forever, until the FPGA start stopping.
    }
    return NULL;
}

/* When device is loaded */
static void virtine_fpga_realize(PCIDevice *pci_dev, Error **errp)
{
    VirtineFpgaDevice *virtine_device = VIRTINEFPGA(pci_dev);
    uint8_t *pci_conf = pci_dev->config;

    // Enable this device to make interrupts
    pci_config_set_interrupt_pin(pci_conf, 1);
    /* Device, offset in config space, number of vectors (must be multiple of 2),
     * 64-bit vectors, MSI per vector mask, error pointer.
     * 0 returned on success. On error, errp and errno has the error code. */
    if (msi_init(pci_dev, 0, 1, true, false, errp)) {
        return;
    }

    memory_region_init_io(&virtine_device->mmio, OBJECT(virtine_device),
                          &virtine_fpga_mmio_ops, virtine_device,
                          "virtine_fpga-mmio", 1 * GiB);

    pci_register_bar(pci_dev, 0, PCI_BASE_ADDRESS_SPACE_MEMORY, &virtine_device->mmio);
    printf("Allocated and registered 1MiB of MMIO space for virtine device @ hardware address 0x%lx\n", (virtine_device->mmio).addr);

    // Set RQ
    reset_queue_pointers(&virtine_device->rq);
    // Set CQ
    reset_queue_pointers(&virtine_device->cq);
    // Set flag registers and batch factor
    virtine_device->doorbell = false;
    virtine_device->is_card_processing = false;
    virtine_device->batch_factor = 1;
    virtine_device->num_virtines_cleaned_already = 0;

    // Set up co-processing thread and its necessary synchronization
    qemu_mutex_init(&virtine_device->processing_lock);
    qemu_cond_init(&virtine_device->processing_condition);
    qemu_thread_create(&virtine_device->processing_thread, "virtine-cleanup-thread",
                       virtine_fpga_virtine_cleanup, virtine_device, QEMU_THREAD_JOINABLE);

    // Create fake clean restoration virtine image
    virtine_device->snapshot_size = sizeof(uint64_t);
    hwaddr *clean_state = g_malloc(virtine_device->snapshot_size);
    *clean_state = 0xfeedbeaddeadbeef;
    virtine_device->snapshot_addr = clean_state;
    // Create fake dirty virtine
    hwaddr *dirty_virtine = g_malloc(virtine_device->snapshot_size);
    *dirty_virtine = 0x123456789abcdef0;
    // Insert the fake dirty virtine to RQ for manual clean-up
    virtine_device->rq.buffer[0] = (hwaddr) dirty_virtine;
    virtine_device->rq.tail_offset = &virtine_device->rq.buffer[1];

    printf("Buildroot physical address size: %lu\n", sizeof(hwaddr));
    printf("Virtine FPGA MMIO Addresses:\n");
    printf("RQ_HEAD_OFFSET_REG: 0x%lx\n", (hwaddr) RQ_HEAD_OFFSET_REG);
    printf("RQ_TAIL_OFFSET_REG: 0x%lx\n", RQ_TAIL_OFFSET_REG);
    printf("RQ_BASE_ADDR: 0x%lx\n", RQ_BASE_ADDR);
    printf("DOORBELL_REG: 0x%lx\n", DOORBELL_REG);
    printf("IS_PROCESSING_REG: 0x%lx\n", IS_PROCESSING_REG);
    printf("CQ_HEAD_OFFSET_REG: 0x%lx\n", CQ_HEAD_OFFSET_REG);
    printf("CQ_TAIL_OFFSET_REG: 0x%lx\n", CQ_TAIL_OFFSET_REG);
    printf("CQ_BASE_ADDR: 0x%lx\n", CQ_BASE_ADDR);
    printf("BATCH_FACTOR_REG: 0x%lx\n", BATCH_FACTOR_REG);
    printf("MAX_NUM_VIRTINES_REG: 0x%lx\n", MAX_NUM_VIRTINES_REG);
    printf("SNAPSHOT_SIZE_REG: 0x%lx\n", SNAPSHOT_SIZE_REG);
    printf("SNAPSHOT_ADDR_REG: 0x%lx\n", SNAPSHOT_ADDR_REG);

    printf("\nVirtine FPGA INTERNAL Addresses:\n");
    printf("RQ_HEAD_OFFSET_REG: 0x%p\n", virtine_device->rq.head_offset);
    printf("RQ_TAIL_OFFSET_REG: 0x%p\n", virtine_device->rq.tail_offset);
    printf("RQ_BASE_ADDR: 0x%p\n", virtine_device->rq.base_addr);
    printf("DOORBELL_REG: 0x%x\n", virtine_device->doorbell);
    printf("IS_PROCESSING_REG: 0x%x\n", virtine_device->is_card_processing);
    printf("CQ_HEAD_OFFSET_REG: 0x%p\n", virtine_device->cq.head_offset);
    printf("CQ_TAIL_OFFSET_REG: 0x%p\n", virtine_device->cq.tail_offset);
    printf("CQ_BASE_ADDR: 0x%p\n", virtine_device->cq.base_addr);
    printf("BATCH_FACTOR_REG: 0x%x\n", virtine_device->batch_factor);
    printf("SNAPSHOT_SIZE_REG: 0x%lx\n", virtine_device->snapshot_size);
    printf("SNAPSHOT_ADDR_REG: 0x%p\n", virtine_device->snapshot_addr);
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

    // Ensure thread stops before killing everything off.
    qemu_mutex_lock(&virtine_device->processing_lock);
    virtine_device->stopping = true;
    qemu_mutex_unlock(&virtine_device->processing_lock);

    qemu_cond_signal(&virtine_device->processing_condition);
    qemu_thread_join(&virtine_device->processing_thread);

    // Free allocated resources
    qemu_cond_destroy(&virtine_device->processing_condition);
    qemu_mutex_destroy(&virtine_device->processing_lock);

    // TODO: Clean up pointers to virtine snapshot stuff

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

/* Reset all ring queue pointers to their default values. */
static inline void reset_queue_pointers(struct virtine_ring_queue *queue)
{
    hwaddr *buffer_base = &queue->buffer[0];
    queue->base_addr = buffer_base;
    queue->head_offset = buffer_base;
    queue->tail_offset = buffer_base;
}

/* Returns the memory location of the last element within the ring queue. */
static inline hwaddr* end_of_queue(struct virtine_ring_queue *queue)
{
    return &queue->buffer[NUM_POSSIBLE_VIRTINES - 1];
}

/* Validate if pointer p is a pointer to within the ring queue. */
static inline bool within(hwaddr *p, struct virtine_ring_queue *queue)
{
    return (p >= queue->base_addr) && (p <= end_of_queue(queue));
}


/* Find the next element of the queue for the specified ring queue pointer, and
 * handle the wrapping of the pointer in the ring buffer.
 * NOTE: This function does NOT actually move the pointer! You must do that!
 * If the provided pointer does not point to somewhere within the provideds ring
 * queue, NULL is returned. */
static hwaddr* next_element(struct virtine_ring_queue *queue, hwaddr *p)
{
    hwaddr *ret = NULL;

    // Validate p is a pointer within the ring queue.
    if(!within(p, queue)) {
        ret = NULL;
    }

    /* If pointer is at last element of queue, then wrap to front.
     * If the pointer is anywhere else, we can just advance forward.
     * REMEMBER: We return the next element, we do NOT change p to there. */
    if(p == end_of_queue(queue)) {
        ret = queue->base_addr;
    } else {
        ret = p + 1; // Typed pointer math, add 1 moves 1*sizeof(hwaddr) space
    }

    return ret;
}

/* Because this is a ring queue, the pointers should never move backwards.
 * Although, we can provide a method to peek at the last element.
 *
 * Find the previous element of the queue for the specified ring queue pointer,
 * and handle the wrapping of the pointer in the ring buffer.
 * If the provided pointer does not point to somewhere within the provideds ring
 * queue, NULL is returned.
 *
 * NOTE: Peeking technically allows someone to view elements before the HEAD
 * pointer of the queue. */
static hwaddr* peek_previous_element(struct virtine_ring_queue *queue, hwaddr *p)
{
    hwaddr *ret = NULL;

    // Valicate p is a pointer within the ring queue.
    if(!within(p, queue)) {
        ret = NULL;
    }

    // If p is at BASE, we go to the end of the list.
    if(p == queue->base_addr) {
        ret = end_of_queue(queue);
    } else {
        /* If TAIL is anywhere else, just move back one spot.
         * Due to typed pointer math, subtracting 1 from something of type hwaddr*
         * will move it back 1 * sizeof(hwaddr), NOT just 1 location. */
        ret = p - 1;
    }

    return ret;
}

/* Fetch and return the data that the queue's HEAD pointer points to. May
 * advance HEAD pointer to the next location. If there is nothing left to pop,
 * then 0 is returned. */
static hwaddr pop_head(struct virtine_ring_queue *queue)
{
    hwaddr ret = *queue->head_offset;

    /* If HEAD is at TAIL, then queue is either completely full or empty.
     * If the queue is full, then the pointers point to real data (a non-zero
     * hwaddr). */
    if((queue->tail_offset != queue->head_offset) ||
       (*queue->head_offset != 0)) {
        // If we pass the ||, then HEAD == TAIL due to short-circuit logic
        *queue->head_offset = 0; // Reset stored hwaddr to invalid value if advancing
        queue->head_offset = next_element(queue, queue->head_offset);
    }

    return ret;
}

/* Take the provided hwaddr and insert at the tail, potentially advancing TAIL.
 * Returns the new address of the tail, if an actual insert occurs.
 * If NO insertion occurs, the return address will be the SAME as the TAIL
 * address before the attempted insertion. */
static hwaddr* insert_tail(struct virtine_ring_queue *queue, hwaddr to_insert)
{
    hwaddr *ret = queue->tail_offset;
    printf("Virtine FPGA: Inserting 0x%lx to %p\n", to_insert, queue->tail_offset);

    /* If HEAD is at TAIL, then queue is either completely full or empty.
     * If the queue is full, then the pointers point to real data (a non-zero
     * hwaddr). */
    if((queue->head_offset != queue->tail_offset) ||
       (*queue->tail_offset == 0)) {
        // If we pass the ||, then HEAD == TAIL due to short-circuit logic
        *queue->tail_offset = to_insert;
        queue->tail_offset = next_element(queue, queue->tail_offset);
        ret = queue->tail_offset; // Update return condition with new TAIL value
    }

    return ret;
}
