---
icon: desktop-download
title: Configuring & Building MOS
order: -2
tags: [build, configure, kconfig, cmake]
---

This guide will walk you through the process of configuring and building MOS from source.

## 1. Initial Configuration (First Time Only)

If it's your first time building MOS, you will need to initialise the build system by running:

```bash
mkdir build && cd build
cmake .. -DMOS_ARCH=x86_64 -GNinja # remove -GNinja if you want to use Make
```

## 2. Adjusting Configurations (Optional)

MOS uses Kconfig, a configuration system that is also used by the Linux kernel, to manage its configuration options.
You can change the configuration options by running:

```bash
ninja menuconfig
```

This will open a menu where you can change the configuration options, the menu is similar to the Linux kernel's menuconfig
and it's even easier to use.

You can also set the configuration options via CMake, see [Kconfig Configuration System](../configure/kconfig.md) for more information.

## 3. Build Targets

You can then examine which targets are available to build by running:

```bash
ninja mos_print_summary
```

A bootable target is a target that produces either a bootable binary or an image that can be started by a bootloader,
or virtual machine. The list of bootable targets can be found in the output of `mos_print_summary` above.

Use `ninja <target>` to build a specific target, e.g. `ninja mos_limine`.

## 4. Utility Targets

There are several utility targets that you can use to build specific parts of MOS:

- `mos_kernel`: Only build the kernel sources into object files, not a single binary or archive would be produced.
- `mos_initrd`: Build the initrd image, which is a CPIO[1] archive that contains the userspace binaries and libraries.
- `mos_cleanup_initrd`: Remove the initrd image, and remove all contents of the `initrd/` directory.
- `mos_print_summary`: Print a summary of the available build targets.

[1]: https://en.wikipedia.org/wiki/Cpio
