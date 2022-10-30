# SPDX-License-Identifier: GPL-3.0-or-later

# This file is part of the MOS project.
# you can change this file according to your needs.

set(MOS_ISA_FAMILY x86)
set(MOS_ISA i686)
set(MOS_BITS 32)

set(MOS_KERNEL_BUILTIN_CMDLINE_STRING "")

set(MOS_MAX_CPU_COUNT 16)
set(MOS_PROCESS_MAX_OPEN_FILES 256)
set(MOS_PROCESS_MAX_THREADS 64)
set(MOS_PAGE_SIZE 4096)
set(MOS_PATH_MAX_LENGTH 256)

set(MOS_USERSPACE_PGALLOC_START 0x40000000)

set(MOS_STACK_PAGES_KERNEL 16)
set(MOS_STACK_PAGES_USER 32)

option(MOS_MEME "Enable MEME" OFF)
option(MOS_MM_LIBALLOC_LOCKS "liballoc: has lock support" OFF)
option(MOS_MM_LIBALLOC_DEBUG "liballoc: debug message enabled" OFF)
option(MOS_DEBUG "Enable debug log" OFF)
option(MOS_PRINTK_HAS_FILENAME "printk: has filename" ON)

# x86 specific options
set(MOS_X86_HEAP_BASE_VADDR 0xD0000000) # Also modify the linker script
set(MOS_X86_INITRD_VADDR 0xC8000000)

if(EXISTS "config.cmake")
    message(STATUS "Loading config.cmake")
    include(config.cmake)
endif()
