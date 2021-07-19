# Virtine Clean-up FPGA Device Emulation #
## Fetch QEMU ##
* QEMU v6.0.0 (609d759652) was used for this development.

```bash
git clone https://github.com/qemu/qemu.git
cd qemu
git checkout v6.0.0
```

## Insert Virtine Clean-up FPGA Device ##
* The `virtine_fpga.c` file must be inserted into an appropriate directory in QEMU.
  - I have chosen `qemu/hw/misc`, but it could be placed elsewhere.
* The `Kconfig` and `meson.build` files must also be edited.
  - See `add-to-build-system.patch` for how this MUST be formatted.
  - This is different than what many other guides online say to do, because QEMU changed to using the Meson build framework for the 6.0.0 release.

## Build QEMU ##
### Dependencies ###
Follow the dependency list given on [QEMU's website](https://wiki.qemu.org/Hosts/Linux).<br>
If you are using Nix and/or NixOS, then you can use the provided `qemu-shell.nix`.

Note that when options are presented to commands, they are given in `man` syntax.
This means:

* `[option]` is an optional option.
* `<required>` is an option that is required to be specified.
* `[option1 | option2]` is an option where either `option1` or `option2` can be chosen.
* `...` is meant to show that some of the display has been clipped has been clipped.

```bash
# Should be in the qemu directory to run this command.
./configure [--target-list=...] # If no targets provided, ALL possible targets are built
make -j <core count> # If -j provided, but no core count, ALL cores will be used
```

When done, a `build` directory is created inside the qemu directory (the path would be `qemu/build`).
Inside of build, there should be a folder named for the target that was built.
If you built all targets, then all possible targets have their own directory.

If you are using the provided `qemu-shell.nix` file to build a subshell with all of the required dependencies, then you can run the block block below instead.
```bash
# Should be in the qemu directory to run this command.
./configure $configureFlags [--target-list=...] # If no targets provided, ALL possible targets are built
make -j <core count> # If -j provided, but no core count, ALL cores will be used
```

## Install (Optional) ##
If you would like to install the output so that you do not need to specify the path to the emulator and make the build permanent, you can run ```sudo make install```.

## Verify ##
Lastly, verify that the emulated virtine clean-up FPGA is available.

```bash
<path-to-built-target> -device ? | grep virtine
```
```
name "virtine-fpga", bus PCI, desc "Virtine FPGA"
```

# Buildroot #
## Dependencies ##
See the list of buildroot's dependencies [here](https://git.buildroot.net/buildroot/tree/docs/manual/prerequisite.txt).

If you are using Nix and/or NixOS, then you can use the provided `buildroot-shell.nix`.

## Build ##
```bash
git clone git://git.buildroot.net/buildroot
```

You can then choose what device you want to target Buildroot at by using:
```bash
make <device>
# For example,
make qemu_x86_64_defconfig
```

Then, build the software
```bash
make -j <core count> # If -j provided, but no core count, ALL cores will be used
```

A directory **output/images/** is created when the build completes.

## Use ##
To use the output buildroot image,
```bash
cd output/images
./start-qemu.sh
```

This should boot the emulated machine with the buildroot root image and display:
```
Welcome to Buildroot
  buildroot login:
```

By default, buildroot creates the root user with _no_ password.

## Use buildroot with Emulated Virtine Clean-up Device ##
To use the output buildroot image **with the Virtine PCI device**, you **must** pass the `-device virtine-fpga` flag to QEMU.
If you installed _your compiled_ QEMU (using `sudo make install`), then you do **NOT** need to specify a path to the QEMU system, and can just use the line below:
```bash
./start-qemu.sh -device virtine-fpga
```

If you did **not** install _your compiled_ QEMU, then you **MUST** specify the path to the compiled QEMU.
```bash
# I assume you are in `buildroot/output/images`
<path/to/qemu/system> -kernel ./bzImage -hda ./rootfs.ext2 -append "rootwait root=/dev/vda console=tty1 console=ttyS0" -net nic,model=virtio -net user -device virtine-fpga

# So, on my development system (when I am at ~/Repos/buildroot/output/images`:
~/Repos/qemu/build/x86_64-softmmu/qemu-system-x86_64 -kernel ./bzImage -hda ./rootfs.ext2 -append "rootwait root=/dev/sda console=tty1 console=ttyS0" -net nic,model=virtio -net user -device virtine-fpga
```

## Include External Kernel Module in Buildroot Kernel ##
Buildroot **requires** at least three files be present for an external package:

1. [`Config.in`](#configin)
2. [`external.mk`](#externalmk)
3. [`external.desc`](#externaldesc)

These files have a ***very*** particular syntax that **MUST** be observed.
A description of this syntax is in the following sections.<br>
Like before, `man`-like syntax is used, in addition, the `#` character being used to denote fields that come from inside files.
This means that `file.name#field` refers to the value of `field` inside `file.name`.

The steps for building a buildroot image that includes the kernel module is as follows:
```bash
make BR2_EXTERNAL="<path/to/package>" <target> # Configuration command
echo "BR2_PACKAGE_<external.desc#name>=y" >> .config # Optional, see Config.in section
make BR2_JLEVEL=<num-cores> -j # -j and BR2_JLEVEL are optional
```

For example, on my system, this looks like:
```bash
# The kernel module is located in this repo, linux-pcie-dma/fpga-char/
make BR2_EXTERNAL="/path/to/linux-pcie-dma/fpga-char" qemu_x86_64_defconfig
# Because I have default y in my Config.in, I do not need to echo
make -j
```

### `Config.in` ###
This configuration file is what buildroot uses to determine what other features must be enabled/disabled for your package (kernel module in this case) to work.
A generic example is provided below.

```conf
config BR2_PACKAGE_<external.desc#name>
	bool <"package-top-level-directory-name">
	[ default y ] <- This line is optional
	depends on BR2_LINUX_KERNEL
	help
	  Linux Kernel Module Cheat.
```

The path string after the `bool` is **INCREDIBLY IMPORTANT** to get right.
In the case of this repository, the kernel module is held in the `linux-pcie-dma/fpga-char/` directory.
Thus, the string after the `bool` **MUST** be `fpga-char`.

If you do **not** specify the `default y` in the configuration, you must also run the following command after running the configuration command:

```bash
echo "BR2_PACKAGE_<external.desc#name>=y" >> .config
```

### `external.desc` ###
Here, there are only two fields that must be filled in:

1. `name:` This is name of the module that buildroot and Kconfig will use throughout their ecosystem.
I recommend that contains **NO** spaces, and limiting yourself to all capital letters and underscore.
2. `desc:` This field is optional.
You can provide a short description (buildroot recommends <40 characters) of the package.

### `external.mk` ###
This defines the internal toolchain that this package requires for it to be built.
There are a minimum of four lines that must be specified, with the fifth technically being optional, but in good style to include.

1. `<external.desc#name>_VERSION` The version of the package, required.
2. `<external.desc#name>_SITE` The location of the package, required.
  * Using `$(BR2_EXTERNAL_<external.desc#name>_PATH` provides an easy way to specify a relocatable path.
3. `<external.desc#name>_SITE_METHOD` Optional if the package source is local, but good to specify even if not.
4. `$(eval $(kernel-module))` Buildroot's build system for building kernel modules, required.
Using this build system requires that the user specify a secondary build system beneath this one.
5. `$(eval $(generic-module))` Buildroot's generic build system for building regular packages, required.
If another build system is more appropriate to use rather than the generic one, the other build system can be substituted for the generic one.

## Enabling Kernel Module Debugging ##
By default, the buildroot kernel sets its `dmesg` logging output fairly low, with `KERN_INFO` and `KERN_DEBUG` not even being printed.
To do this, find your board's respective file in the `buildroot/boards/` directory, and edit the configuration file to include:

```bash
CONFIG_DEBUG_KERNEL=y
CONFIG_DEBUG_INFO=y
CONFIG_DYNAMIC_DEBUG=y
```

Then, the logging level can be increased at boot-time by passing another command-line parameter to the kernel, using the `-append` string.

```bash
<path/to/qemu/system> -kernel ./bzImage -hda ./rootfs.ext2 -append "rootwait root=/dev/vda console=tty1 console=ttyS0 loglevel=<desired_level>" -net nic,model=virtio -net user -device virtine-fpga
# Or you can print ALL kernel messages
<path/to/qemu/system> -kernel ./bzImage -hda ./rootfs.ext2 -append "rootwait root=/dev/vda console=tty1 console=ttyS0 ignore_loglevel" -net nic,model=virtio -net user -device virtine-fpga
```

For logging, the number provided is the upper-limit that the kernel module will output.
This means that if you specify `loglevel=6` you will only receive kernel messages from log levels 0 to 5.
0. `KERN_EMERG`
1. `KERN_ALERT`
2. `KERN_CRIT`
3. `KERN_ERR`
4. `KERN_WARNING`
5. `KERN_NOTICE`
6. `KERN_INFO`
7. `KERN_DEBUG`
