# MOS Development Prerequisites

MOS is an operating system, thus, preparing for a fully-functional development
environment is not an easy task (bruh). Several efforts have been made to make
the process easier.

The following steps are required on a Linux machine, for Windows and macOS users,
~~it's not tested yet but theoretically it should work~~.

Basically, to set up the development environment for MOS (for now, only x86 target
exists), several tools are required:

- CMake & (Ninja or Make)
- **i686-elf**-binutils, **i686-elf**-gcc and **i686-elf**-gdb
- NASM
- cpio (for initcpio, containing the very first readonly root filesystem)
- qemu-system-i386 (for running the OS in an emulator)

We'll discuss about each of them in the following sections.

## CMake & (Ninja or Make)

CMake is a cross-platform build system generator, which generates (e.g. Makefiles)
for a build system (e.g. GNU Make) to use.

MOS uses CMake instead of GNU Make directly, because CMake is more flexible and
powerful in many aspects.

We'll come back to CMake in the later chapters.

## i686-elf-binutils, i686-elf-gcc and i686-elf-gdb

As its name suggests, this is a cross-compiler toolchain for "i686-elf" target.
"i686" means the 32-bit x86 architecture, and "elf" means the executable format.

> You may (or may not) know that most 64-bit Linux uses a target triple of "x86_64-pc-linux-gnu".

Unlike other applications (e.g. bash or vim) that they run on an existing operating
system and a standard libc (say, glibc or musl). MOS itself is the operating system,
thus there's not an existing OS for it to run on, neither a standard libc (i.e. no
`printf`, no `malloc` etc.) for it to use.

A "bare-metal" compiler toolchain is exactly for this situation. Considering you're
directly interacting with the CPU and the hardware.

There are majorly two ways to get this toolchain, either by building it from source
or by downloading a pre-built binary.

### Downloading a pre-built binary

> For Arch Linux users, checkout PKGBUILDs for [i686-elf-binutils](https://github.com/moodyhunter/repo/blob/main/moody/i686-elf-binutils/PKGBUILD), [i686-elf-gcc](https://github.com/moodyhunter/repo/blob/main/moody/i686-elf-gcc/PKGBUILD), [i686-elf-gdb](https://github.com/moodyhunter/repo/blob/main/moody/i686-elf-gdb/PKGBUILD). They _should_ be conforming to the Arch Linux Packaging Standards.

If you don't want to build the toolchain from source, you can download a pre-built
binary from [moodyhunter/i686-elf-prebuilt](https://github.com/moodyhunter/i686-elf-prebuilt/releases/tag/artifact) (do choose the i686 one).

> Note: Using the pre-built binary is a lot easier, but **only** doing so if you trust the author. (which is me)

### Building from source

The source code of binutils and gcc can be found at [GNU's website](https://www.gnu.org/software/binutils/) and [GCC's website](https://gcc.gnu.org/), respectively.

The following script downloads, compiles binutils, gcc and gbd into `PREFIX`:

```bash
# PLEASE do replace this with your own path, I don't want to mess up your home directory
export PREFIX="$HOME/mos-cross-toolchain"
export MAKEFLAGS="-j$(nproc)"       # Change this to your liking

export BINUTILS_VERSION="2.39"      # The latest version at the time of writing
export GCC_VERSION="12.2.0"         # The latest version at the time of writing
export GDB_VERSION="12.1"           # The latest version at the time of writing
export TARGET="i686-elf"            # The target triple


mkdir -p "$PREFIX"
cd "$PREFIX"
mkdir _work && cd _work

# Download and compile binutils
wget https://ftp.gnu.org/gnu/binutils/binutils-$BINUTILS_VERSION.tar.gz
tar -xf binutils-$BINUTILS_VERSION.tar.gz

cd binutils-$BINUTILS_VERSION
mkdir build && cd build
../configure \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --disable-nls \
    --disable-werror

make && make install
cd ../..

# Download and compile GCC
wget https://ftp.gnu.org/gnu/gcc/gcc-$GCC_VERSION/gcc-$GCC_VERSION.tar.gz
tar -xf gcc-$GCC_VERSION.tar.gz

cd gcc-$GCC_VERSION
mkdir build && cd build
../configure \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --disable-nls \
    --disable-werror \
    --without-headers \
    --enable-libgcc \
    --enable-languages=c,c++ \
    --disable-build-format-warnings

# This will take a **long** time
make all-gcc all-target-libgcc
make install-gcc install-target-libgcc
cd ../..

# Download and compile GDB
wget https://ftp.gnu.org/gnu/gdb/gdb-$GDB_VERSION.tar.gz
tar -xf gdb-$GDB_VERSION.tar.gz

cd gdb-$GDB_VERSION
mkdir build && cd build
../configure \
    --target="$TARGET" \
    --prefix="$PREFIX" \
    --program-prefix="$TARGET-"

make && make install
cd ../..
cd .. # back to $PREFIX
```

A similar script can be found at <https://github.com/moodyhunter/i686-elf-prebuilt>

## NASM

NASM is an assembler for x86 architecture. There are several files under `arch/x86`
that are written in NASM.

NASM can be installed via your Linux's package manager, (e.g. `sudo apt install nasm`
or `sudo pacman -S nasm`). Or you can download a pre-built binary from [NASM's website](https://www.nasm.us/).

## cpio

"cpio" is a tool to create archives in a simple format. MOS uses cpio as the initial
root filesystem.

Similar to NASM, you can install cpio via both package manager like `sudo apt install cpio`
or `sudo pacman -S cpio`, downloading and compiling it from source is also possible, but is not discussed here.

## qemu-system-i386

QEMU is an open-source emulator, it also provides a gdb stub for debugging.

QEMU can be installed via your Linux's package manager, (e.g. `sudo apt install qemu-system-i386`
or `sudo pacman -S qemu-system-x86`). See its [download page](https://www.qemu.org/download) for more details.
