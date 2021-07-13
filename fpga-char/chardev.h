#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/io.h>

#include "modinfo.h"
#include "fpga_char_main.h"

#define MAX_MINOR_DEVICES 1

int create_char_devs(struct fpga_device *fpga);
int destroy_char_devs(void);

/* The magic 'F' has MANY drivers. Some other sequence numbers (the second param)
 * are taken. I use between 0x30 and 0x80 to give myself room to experiment.
 * To define a new ioctl number, I recommend you use one of the 4 macros below:
 * _IO(magic, number) - No inputs/outputs
 * _IOR(magic, number, input_data_type) - ioctl with input
 * _IOW(magic, number, output_data_type) - ioctl with output
 * _IORW(magic, number, in_out_data_type) - ioctl with input and output
 * ALL ioctls THAT TAKE DATATYPE PARAMETERS ONLY TAKE THE PARAMETER!!
 * i.e. _IOR(magic, number, struct struct_name), NOT
 *      _IOR(magic, number, sizeof(struct struct_name)).
 * Note that the struct is limited to a maximum of 16KiB (14 address bits) */
#define IOCTL_MAGIC 'F'

#define FPGA_CHAR_MODIFY_BATCH_FACTOR _IOR(IOCTL_MAGIC, 0x30, unsigned long)
#define FPGA_CHAR_GET_MAX_NUM_VIRTINES _IOW(IOCTL_MAGIC, 0x31, unsigned long*)
#define FPGA_CHAR_RING_DOORBELL _IO(IOCTL_MAGIC, 0x32)

#endif
