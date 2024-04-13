---
icon: desktop-download
title: Configuring & Building MOS
order: -2
---

## 1. Configuring

Configuring MOS is simply a matter of running these commands:

```bash
mkdir build && cd build
cmake .. -DMOS_ARCH=x86_64 -GNinja # remove -GNinja if you want to use Make
```

You can then examine which targets are available to build by running:

```bash
ninja mos_print_summary
```

## 2. Bootable Targets

A bootable target is a target that produces either a bootable binary or an image that can be started by a bootloader,
or virtual machine. The list of bootable targets can be found in the output of `mos_print_summary` above.

Use `ninjia <target>` to build a specific target, e.g. `ninja mos_limine`.
