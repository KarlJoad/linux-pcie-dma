#ifndef CHARDEV_H
#define CHARDEV_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h> // For put_user and get_user

#include "modinfo.h"

int create_char_devs(void);
int destroy_char_devs(void);

#endif
