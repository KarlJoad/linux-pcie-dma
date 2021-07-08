#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <asm/io.h>

#include "modinfo.h"
#include "fpga_char.h"

#define MAX_MINOR_DEVICES 1

int create_char_devs(struct fpga_device *fpga);
int destroy_char_devs(void);

/* The magic 'F' has MANY drivers. Some other sequence numbers (the second param)
 * are taken. I use between 0x30 and 0x80 to give myself room to experiment. */
#define FPGA_CHAR_FEEDBEAD _IO('F', 0x30) // TODO: REMOVE THIS PLACEHOLDER NUM

#endif
