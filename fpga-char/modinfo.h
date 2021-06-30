#ifndef MODINFO_H
#define MODINFO_H

#include <linux/module.h>

#define MODULE_NAME "PCIe FPGA Character Device"
#define DEVICE_NAME "PCI Char FPGA"

MODULE_AUTHOR("Karl Hallsby <karl@hallsby.com>");
MODULE_DESCRIPTION("PCI-based char device interface to FPGA.");
MODULE_LICENSE("GPL");

#endif
