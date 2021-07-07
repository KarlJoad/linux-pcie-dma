VIRTINE_FPGA_KERNEL_MODULE_VERSION = 1.0
VIRTINE_FPGA_KERNEL_MODULE_SITE = $(BR2_EXTERNAL_VIRTINE_FPGA_KERNEL_MODULE_PATH)
VIRTINE_FPGA_KERNEL_MODULE_SITE_METHOD = local
VIRTINE_FPGA_KERNEL_MODULE_INSTALL_IMAGES = YES

BINARY := fpga_char
OBJECTS := $(BINARY)_main.o \
           chardev.o
obj-m += $(BINARY).o
$(BINARY)-objs := $(OBJECTS)

ccflags-y := -DDEBUG -g -std=gnu99 -Wno-declaration-after-statement

all:
	$(MAKE) -C '$(LINUX_DIR)' M='$(VIRTINE_FPGA_KERNEL_MODULE_SITE)' O='$(LINUX_DIR)/lib/$(LINUX_VERSION_PROBED)' modules_install

$(eval $(kernel-module))
$(eval $(generic-package))
