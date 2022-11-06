# Getting Started

## Setting up the development environment

To set up the development environment for MOS (for now, only x86 target exists), several tools are required:

- **i686-elf**-gcc
- CMake
- NASM
- cpio
- qemu-system-i386
- _(optional)_ grub-mkrescue
- _(optional)_ gdb

We'll discuss about each of them in the following sections.

### i686-elf-gcc

As its name suggests, this is a cross-compiler for "i686-elf" target. It's effectively a bare-metal compiler (i.e. there's no libc or other libraries included), read more about target triplets [here](https://wiki.osdev.org/Target_Triplet)
