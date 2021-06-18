#include <linux/module.h>
#include <linux/kernel.h>

#define MODULE_NAME "Hello_World"
#define DRIVER_AUTHOR "Karl Hallsby <karl@hallsby.com>"
#define DRIVER_DESCRIPTION "Small \"Hello World\" to practice kernel module \
development."

static int __init hello_init(void) {
        printk(KERN_INFO "Hello world!\n");

        /* Non-zero here means the module will fail to continue loading. */
        return 0;
}

static void __exit hello_exit(void) {
        printk(KERN_INFO "Goodbye world!\n");
}

module_init(hello_init);
module_exit(hello_exit);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_LICENSE("GPL");
