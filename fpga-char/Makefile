BINARY := fpga_char
OBJECTS := $(BINARY)_main.o \
           chardev.o

obj-m += $(BINARY).o

$(BINARY)-objs := $(OBJECTS)
# $(BINARY)-y := $(OBJECTS)

# Get LINUX_DEV env var
LINUX_DEV_DIR = $(strip ${LINUX_DEV})

MODULE_BUILD_FLAGS =-j 16
C_FLAGS := -Wall -DDEBUG -g -std=gnu99 -Wno-declaration-after-statement
ccflags-y += $(C_FLAGS)

CFLAGS_$(BINARY).o := -DDEBUG

.PHONY: all clean

all:
	$(MAKE) -C '$(LINUX_DIR)' M='$(PWD)' modules

clean:
	$(MAKE) -C '$(LINUX_DIR)' M='$(PWD)' clean

# The host-* targets below are for building normally, without buildroot
host-build:
	make $(MODULE_BUILD_FLAGS) -C $(LINUX_DEV_DIR)/lib/modules/*/build M=$(PWD) modules

host-clean:
	make $(MODULE_BUILD_FLAGS) -C $(LINUX_DEV_DIR)/lib/modules/*/build M=$(PWD) clean
