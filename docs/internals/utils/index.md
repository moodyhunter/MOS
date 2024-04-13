---
icon: tools
title: Internal Utilities
order: -2
description: >-
  This section describes internal utilities used in MOS, how they work, and how they helped in the development of the kernel.
---

Several internal utilities are used in MOS to help with the development, debugging, and testing of the kernel.

## 1. Logging

MOS uses a similar logging system to the one used in the Linux kernel, based on `printk` function, extended with `pr_[d]{info,info2,emph,warn,emerg,fatal,cont}` macros for different log levels.

MOS also has its own extension to the standard format specifiers, such as `%pvm` for printing a virtual mapping object.

For more information, see the [Logging](logging.md) document.

## 2. Kernel Symbol Table

MOS has a symbol table that is used to store the addresses of kernel functions. This is extremely useful for debugging, as it allows the kernel to print the call stack when something goes wrong.

For more information, see the [Kernel Symbol Table (kallsyms)](kallsyms.md) document.
