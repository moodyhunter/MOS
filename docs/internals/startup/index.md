---
icon: upload
title: Startup Process
order: -1
description: >-
  This section  describes the startup process of MOS, from the bootloader to the creation of the first user process.
---

This section describes the startup process of MOS, from the bootloader to the creation of the first user process.

## 1. Command Line Parsing

`cmdline` is important for the kernel, as it allows the user to pass arguments to the kernel at runtime. The kernel parses the command line and uses the arguments to configure itself.

Details about how `cmdline` is parsed, and evaluated can be found in the [Command Line](cmdline.md) section.
