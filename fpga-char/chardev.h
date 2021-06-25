#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>

#include "modinfo.h"
#include "fpga_char_main.h"

#define MAX_MINOR_DEVICES 1

int create_char_devs(struct fpga_device *fpga);
int destroy_char_devs(void);

#endif