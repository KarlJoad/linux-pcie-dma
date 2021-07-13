#ifndef PCIE_CHAR_H
#define PCIE_CHAR_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>

/* This is a "private" struct, meaning the kernel does not provide or interact
 * with this struct in any way. This is supposed to be a software-side definition
 * of the required components that the driver/module can/should use to complete
 * its task. */
struct fpga_device {
        u16 vendor_id;
        u16 device_id;
        u8 __iomem *dev_mem; // Pointer to mmap-ed device BAR in host's memory.
};

/* MMIO DESIGN (Remember PCI is little-endian):
 * 0x0                               0x8
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
#define MAX_NUM_VIRTINES_REG BATCH_FACTOR_REG + (sizeof(unsigned long))

#endif
