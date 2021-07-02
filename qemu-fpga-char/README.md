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
* The `Kconfig` and `meson.config` files must also be edited.
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
~/Repos/qemu/build/x86_64-softmmu/qemu-system-x86_64 -kernel ./bzImage -hda ./rootfs.ext2 -append "rootwait root=/dev/vda console=tty1 console=ttyS0" -net nic,model=virtio -net user -device virtine-fpga
```
