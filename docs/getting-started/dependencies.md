---
icon: package-dependencies
title: Dependencies
order: -1
tags: [dependencies, build, toolchain, packages]
---

In this section, you will find the dependencies required to build the **MOS** kernel and userspace.

## 1. Build Tools

- [CMake](https://cmake.org/download/) 3.20 or later
- [Ninja](https://ninja-build.org/) or [Make](https://www.gnu.org/software/make/), Ninja is recommended for faster builds
- [Python 3+](https://www.python.org/downloads/), required for the Kconfig tool and system call generator

You can install these tools using your favorite package manager.

## 2. Architecture Specific Dependencies

**x86_64**: please refer to the [x86_64 dependencies](../arch/x86_64/dependencies.md) for the dependencies.

## 3. MOS Toolchain

You will need a cross-compiler toolchain to build the kernel and userspace for MOS. The toolchain must be specifically
for the target architecture, which can only be `x86_64` at the moment.

Suppose that `$ARCH` is the target architecture, e.g. `x86_64`, you will need the following packages:

- `mos-sdk` for CMake support for the MOS operating system.
- `$ARCH-mos-binutils` and `$ARCH-mos-gcc` for the cross-compiler toolchain, which includes the assembler, linker, and compiler.
- `$ARCH-mos-mlibc-headers` or `$ARCH-mos-mlibc` for the C standard library headers and/or implementation.

### 3.1. Choosing between `mlibc-headers` and `mlibc`

- `mlibc-headers` is a package that only contains the headers of the C standard library, which is useful for building the kernel only.
- `mlibc` contains both the headers and the implementation of mlibc, this is required for building the userspace applications.

### 3.2. Installing the Toolchain

MOS uses `pacman` as its package manager, which is the one used in Arch Linux, MSYS2 and other distributions. The pacman
package manager is simple and easy to use. The packaging scripts are written in bash, making it easy to create and maintain
packages.

+++ Binary Package Repository

You will be able to find these packages from the [MOS Binary Repository](https://mospkg.mooody.me), simply add the following
repository to your `pacman.conf`:

```ini
# Packages are not signed yet
[mos]
SigLevel = Optional
Server = https://mospkg.mooody.me/$arch
```

Then you can install the packages using:

```bash
export ARCH=x86_64 # or any other supported architecture
pacman -S mos-sdk $ARCH-mos-binutils $ARCH-mos-gcc $ARCH-mos-mlibc
```

+++ Building from Source
This option is more useful if you want to customize the toolchain, or if you want to build the toolchain for an unsupported
architecture.

The toolchain is built using the `makepkg` tool, which is part of the `pacman` package manager.

You can find the packaging scripts for the toolchain in the [MOS Package Repository](https://github.com/moodyhunter/mos-packages).
+++

### 3.3. Using the Toolchain

Once installed, the libraries will be available at `/opt/$ARCH-mos` while the cross compilers will be installed to the
standard locations, e.g. `/usr/bin/$ARCH-mos-gcc`.

## 4. After Installation

No further configuration is required to use the toolchain, once you have installed the packages, you can start
[configuring and building MOS](configure-build.md).
