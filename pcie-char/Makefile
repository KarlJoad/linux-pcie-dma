BINARY := pcie_char
OBJECTS := pcie_char.o \
           chardev.o

obj-m += $(BINARY).o

# Get LINUX_DEV env var
LINUX_DEV_DIR = $(strip ${LINUX_DEV})

MODULE_BUILD_FLAGS =-j 16
C_FLAGS := -Wall
ccflags-y += $(CFLAGS)

all:
	make $(MODULE_BUILD_FLAGS) -C $(LINUX_DEV_DIR)/lib/modules/*/build M=$(PWD) modules

clean:
	make $(MODULE_BUILD_FLAGS) -C $(LINUX_DEV_DIR)/lib/modules/*/build M=$(PWD) clean
