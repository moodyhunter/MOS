---
icon: upload
title: Startup Process
order: -1
description: >-
  This section  describes the startup process of MOS, from the bootloader to the creation of the first user process.
---

## 1. Bootloader

The very first stage of startup is the phase of bootloader, which is **limine** only for now, the bootloader will:

- Map correct ELF segments into memory and setup correct permissions
- Read and sanitise the memory layout provided by the firmware
- Map initrd, load cmdlines
- Setup direct mapping, which maps the entire physical memory space to a specific range of virtual memory.
- Potentially:
  - Start secondary CPUs (APs)
  - Find ACPI tables
  - Find firmware-provided device tree

## 2. Command Line Parsing

`cmdline` is important for the kernel, as it allows the user to pass arguments to the kernel at runtime. The kernel parses the command line and uses the arguments to configure itself.

Details about how `cmdline` is parsed, and evaluated can be found in the [Command Line](cmdline.md) section.
