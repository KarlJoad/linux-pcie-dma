# What is this? #
This repo contains all the programs, kernel modules, and QEMU devices required to build and use a dedicated asynchronous co-processor to clean up virtines.

The commit history of this repository is likely quite messy.
At the start of working on this project, I did not have *any* experience writing kernel modules, QEMU devices, or working with Buildroot.
So, there was a lot of testing and fiddling with the way pieces fit together to make everything work.

If you notice something that seems particularly egregious, let me know.

# Directory Discussion #
Here, the various subdirectories within this repository are discussed, and if necessary, some explanation given.

## `example` ##
Here, an example of a kernel module from a Xilinx support forum is give.
This module creates a character device and its `ioctl` interface from both the kernel's perspective and the userland perspective.

## `fpga-char-test` ##
This directory provides some programs that can be used to prove the QEMU virtine co-processor.
Because I used [Buildroot](https://buildroot.org/) to build an *incredibly* minimal root filesystem for the QEMU virtual machine, many common tools that you may usually find present are not.
So, to provide some debugging and testing support from within the virtual machine, these programs can be compiled and built into the Buildroot environment to query the state of the device.

### NOTE ###
These programs work as they should.
They are **not** designed to be stable.
There may be memory leaks, there may be files that are still open when the program exits, etc.
I coded these programs to be debugging tools from *inside* a virtual machine; I figured that it was more important that I got the information from the FPGA device and kernel module quickly rather than do it cleanly.

## `fpga-char` ##
This is the actual kernel module for the FPGA device.
When built, it will output a kernel object that can be `insmod`ed to the currently running kernel.

I have tried to write the module in such a way that it can be statically compiled into the kernel *or* be dynamically loaded.
However, I make **no** promises about it being statically compile-able.

The module uses multiple source files and the `Makefile` pulls everything back together.

### Buildroot ###
This module can be built using Buildroot's build system as well.
The `external.mk`, `external.desc`, and `Config.in` are all used for that.

To learn how to compile this kernel module into a Buildroot environment, follow the instructions in the [`qemu-fpga-char`](https://github.com/KarlJoad/linux-pcie-dma/blob/master/qemu-fpga-char/README.md) directory.

## `hello-world` ##
This a hello world kernel module.
This was the first thing I started working on when learning how to write kernel modules.
All it does is print `Hello World` when it is inserted, and `Goodbye World` when it is removed.

I also tinkered with using [Nix](https://nixos.org/manual/nix/stable/#nix-package-manager-guide) to reproducibly build the kernel module.
It does not produce a truly reproducible module, but it at least ensures that everything will compile.
The output kernel object may not work with your kernel due to version differences between the kernel version fetched from the Nix binary cache and the kernel you are currently running.

## `pcie-echo` ##
This is a first attempt at a kernel module that interacts with a PCI device.
It is intended to map the BAR memory space into the kernel, so that it can be worked with.
This was mainly to test and learn how to write a kernel module that interacted with real hardware.

At the time, this design used the Intel IP for the Cyclone 10 GX Development Kit that come swith Intel Quartus Prime Pro.
This design includes two differently sized memory regions under BARs 1 and 3.
When a write occurred to the device, it was written to one BAR's memory location, and the write was verified to have occurred by viewing the memory directly using `mmap` on `/dev/mem/`.

## `qemu-fpga-char` ##
This is the repository that discusses how to build QEMU, Buildroot, and the PCI device that I created.
The device is a single-virtine clean-up device.
It takes in a snapshot of a virtine to use and performs an action that appears very similar to a `memcpy` to return a virtine that has performed some computation to its snapshot state.

## `sample-char` ##
This was a first attempt at a kernel module that produced a usable character device in the kernel.
It was intended to be a FIFO, where whomever performed a `write` to the device could `read` whatever was written to the device back out.
But, it is a **very** poor character device and is not a good representation of one *should* look like.

# Dependencies
The dependencies for each of the subdirectories is handled given inside of the subdirectory.
Look in there to find them.
To make things easier for myself, and potentially for you, the various `*shell.nix` files provide a list of all dependencies required to make these programs compile.
It goes without saying that Linux (headers and a running kernel), GCC, GNU Make, and some GNU Auto-tools are also required.
