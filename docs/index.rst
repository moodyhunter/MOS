.. contents::
   :depth: 3
..

.. container:: titlepage

   .. container:: center

      .. image:: logo/logo-no-background.png
         :alt: image
         :width: 50.0%

   .. container:: center

      | **The MOS Project**
      | **Lab Tutorial and Documentation**

      | Moody Liu
      | November 2022

Lab Tutorial
============

Lab Introduction
----------------

The primary goal of MOS is to provide an platform-independent
environment for beginners to learn OS.

Users of MOS will be able to experiment and get them familiar with with
these following features of an operating system:

*Note:* The **bold** items are the main objectives of the project.

-  Basic OS Knowledges

-  Memory Management **Paging**, and a Brief of Segmentation Common
   Memory Management Practices in Modern Kernels

-  File Systems Underlying File Operations VFS Framework (a file-system
   abstraction layer)

-  Process Management Allocating a New Process The Famous ``fork()``
   Syscall **Process Scheduler** **Threads** **Threads Synchronization**
   via ``mutex``

Lab 1 - Setting Up the Development Environment
----------------------------------------------

MOS is an operating system, thus, preparing for a fully-functional
development environment is not an easy task (bruh). Several efforts have
been made to make the process easier.

To set up the development environment for MOS (well, for now, only x86
target exists), several tools are required:

-  CMake and (Ninja or Make)

-  i686-elf-binutils, i686-elf-gcc and i686-elf-gdb

-  NASM

-  cpio

-  qemu-system-i386

We’ll discuss about each of them in the following sections.

CMake and (Ninja or Make)
~~~~~~~~~~~~~~~~~~~~~~~~~

CMake is a cross-platform build system generator, which generates (e.g.
Makefiles) for a build system (e.g. GNU Make) to use.

MOS uses CMake instead of GNU Make directly, because CMake is more
flexible and powerful in many aspects.

We’ll come back to CMake in the later chapters.

i686-elf-{binutils,gcc,gdb}
~~~~~~~~~~~~~~~~~~~~~~~~~~~

As its name suggests, this is a cross-compiler toolchain for ‘i686-elf’
target. ‘i686’ means the 32-bit x86 architecture, and ‘elf’ means the
executable format.

.. container:: mdframed

   **Tip:** Most 64-bit Linux has a target triple of
   ‘x86_64-pc-linux-gnu’.

Unlike other applications (e.g. bash or vim) that they run on an
existing operating system and a standard libc (say, glibc or musl). MOS
itself is the operating system, thus there’s not an existing OS for it
to run on, neither a standard libc (i.e. no ``printf``, no ``malloc``
etc.) for it to use.

A ‘bare-metal’ compiler toolchain is exactly for this situation.
Considering you’re directly interacting with the CPU and the hardware.

There are majorly two ways to get this toolchain, either by building it
from source or by downloading a pre-built binary.

Downloading a pre-built binary
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

If you don’t want to build the toolchain from source, you can download a
pre-built binary from
`moodyhunter/i686-elf-prebuilt <https://github.com/moodyhunter/i686-elf-prebuilt/releases>`__
(choose the i686 one).

.. container:: mdframed

   **Warning:**

   -  Using pre-built binary saves time, but please consider doing so
      **only** if you trust the author.

   -  The above pre-built binary is built with GitHub Actions, and is
      built on Ubuntu 20.04.5 LTS (Image ``ubuntu-20.04`` version
      ``20221027.1``).

Building From Source
^^^^^^^^^^^^^^^^^^^^

The source code of binutils and gcc can be found at `GNU Binutils’s
Website <https://www.gnu.org/software/binutils>`__ and `GNU GCC’s
Website <https://gcc.gnu.org>`__ respectively.

.. container:: mdframed

   **Note:** For Arch Linux users, checkout
   `i686-elf-binutils <https://github.com/moodyhunter/repo/blob/main/moody/i686-elf-binutils/PKGBUILD>`__,
   `i686-elf-gcc <https://github.com/moodyhunter/repo/blob/main/moody/i686-elf-gcc/PKGBUILD>`__
   and
   `i686-elf-gdb <https://github.com/moodyhunter/repo/blob/main/moody/i686-elf-gdb/PKGBUILD>`__.

The script located at ``docs/assets/i686-elf-toolchain.sh`` downloads,
compiles and installs them into the directory specified by the
``PREFIX`` variable.

.. code:: octave

   export PREFIX="/tmp"           # <--- CHANGE THIS
   export MAKEFLAGS="-j$(nproc)"  # Change this to your liking
   export BINUTILS_VERSION="2.39" # The latest version at the time of writing
   export GCC_VERSION="12.2.0"    # The latest version at the time of writing
   export GDB_VERSION="12.1"      # The latest version at the time of writing
   export TARGET="i686-elf"       # The target triple

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
   -cd .. # back to $PREFIX

NASM
~~~~

NASM is an assembler for x86 architecture. There are several files under
‘arch/x86‘ that are written in NASM.

NASM can be installed via your Linux’s package manager, (e.g. ‘sudo apt
install nasm‘ or ‘sudo pacman -S nasm‘). Or you can download a pre-built
binary from [NASM’s website](https://www.nasm.us/).

cpio
~~~~

cpio is the tool to create archives in a the cpio format. MOS uses cpio
as the initial root filesystem.

Similar to NASM, you can install cpio via both package manager like
‘sudo apt install cpio‘ or ‘sudo pacman -S cpio‘, downloading and
compiling it from source is also possible, but is not discussed here.

qemu-system-i386
~~~~~~~~~~~~~~~~

QEMU is an open-source emulator, it also provides a gdb stub for
debugging.

QEMU can be installed via your Linux’s package manager, (e.g. ‘sudo apt
install qemu-system-i386‘ or ‘sudo pacman -S qemu-system-x86‘). See its
[download page](https://www.qemu.org/download) for more details.

Documentation
=============
