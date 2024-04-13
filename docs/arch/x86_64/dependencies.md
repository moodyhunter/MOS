---
label: Build Dependencies
title: Build Dependencies for x86_64 Architecture
icon: package-dependencies
---

## 1. Limine Bootloader

The only supported bootloader protocol for MOS on x86_64 is [Limine](https://github.com/limine-bootloader/limine).
You should have limine installed on your system to build the MOS kernel.

During the build process, the `limine` binary will be invoked to create a bootable disk image, and
`xorriso` will be used to create an ISO image.
