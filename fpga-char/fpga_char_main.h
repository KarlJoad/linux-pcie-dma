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

#endif
