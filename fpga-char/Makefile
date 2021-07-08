BINARY := fpga_char
OBJECTS := $(BINARY).o \
           chardev.o

obj-m += $(BINARY).o

$(BINARY)-objs := $(OBJECTS)

# Get LINUX_DEV env var
LINUX_DEV_DIR = $(strip ${LINUX_DEV})

MODULE_BUILD_FLAGS =-j 16
C_FLAGS := -Wall -DDEBUG -g -std=gnu99 -Wno-declaration-after-statement
ccflags-y += $(CFLAGS)

all:
	make $(MODULE_BUILD_FLAGS) -C $(LINUX_DEV_DIR)/lib/modules/*/build M=$(PWD) modules

clean:
	make $(MODULE_BUILD_FLAGS) -C $(LINUX_DEV_DIR)/lib/modules/*/build M=$(PWD) clean
