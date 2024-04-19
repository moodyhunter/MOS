---
title: Kconfig Configuration System
order: -1
tags: [configure, kconfig, cmake]
---

This document describes how to configure MOS using the Kconfig configuration system. For more information on Kconfig, the configuration system itself, please refer to the [Kconfig documentation](https://www.kernel.org/doc/html/latest/kbuild/kconfig.html).

## 1. menuconfig

The `menuconfig` tool used by MOS is implemented in Python by Zephyr, you can use `ninja menuconfig` to open the
configuration menu.

**Several useful keys are:**

- `↑` and `↓`: Move the cursor up and down.
- `→` and `←`: Enter and exit a submenu.
- `Space`: Toggle the selected option.
- `h`: Display help and symbol dependencies.
- `/`: Search for a symbol, the search result list updates in real-time.

## 2. Setting Configuration Options via CMake

All parameters that can be set via `menuconfig` can also be set via CMake command lines. For example, to enable
`MM_DETAILED_UNHANDLED_FAULT` for detailed information about an unhandled page fault, you can run:

```bash
cmake . -DMOS_MM_DETAILED_UNHANDLED_FAULT=y
```

!!! warning
The configuration options are case-insensitive, so you can use either `y` or `Y` to enable an option.
However, `ON` is not a valid value for boolean options.
!!!

Non-boolean options are also set in the same way:

```bash
cmake . -DMOS_MAX_MEMREGIONS=12 -DMOS_RUST_TARGET="x86_64-unknown-mos"
```

You can press `c` to display the configuration name in menuconfig.
