# SPDX-License-Identifier: GPL-3.0-or-later

# ! This file contains the default configuration for MOS.
# ! To override the default values, use CMake's -D option instead of editing this file.

add_summary_section(BOOTABLE    "Bootable Targets")
add_summary_section(DEBUG       "Platform-Independent Kernel Debugging")
add_summary_section(HARDWARE    "Hardware Support")
add_summary_section(INITRD      "Initrd Items")
add_summary_section(KERNEL      "Kernel Features")
add_summary_section(LIMITS      "Limits")
add_summary_section(MISC        "Miscellaneous")
add_summary_section(MM          "Memory Management")
add_summary_section(TESTS       "Kernelspace Unit Tests")
add_summary_section(USERSPACE   "Userspace Targets")
add_summary_section(UTILITY     "Utility Targets")

add_summary_item(DEBUG      MOS_DEBUG_ALL               OFF "Enable all debugging features")
mos_kconfig(DEBUG           MOS_PRINTK_WITH_FILENAME    OFF "Print filename in printk")

if(NOT DEFINED MOS_DEBUG_ALL)
    set(MOS_DEBUG_ALL OFF)
endif()

macro(mos_debug feature description)
    mos_kconfig(DEBUG MOS_DEBUG_${feature} ${MOS_DEBUG_ALL} ${description})
endmacro()

if (NOT DEFINED MOS_ARCH)
    set(MOS_ARCH "x86") # The only supported architecture for now
endif()

set(_ARCH_CONFIGURATION_FILE ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH}/platform_config.cmake)
set(MOS_ARCH_CONFIG ${CMAKE_SOURCE_DIR}/arch/${MOS_ARCH}/mos_platform.cmake)

if(NOT EXISTS ${_ARCH_CONFIGURATION_FILE} OR NOT EXISTS ${MOS_ARCH_CONFIG})
    message(FATAL_ERROR "Architecture '${MOS_ARCH}' is not supported, or the configuration file is missing.")
endif()

include(${_ARCH_CONFIGURATION_FILE})

if(NOT DEFINED MOS_COMPILER_PREFIX OR NOT DEFINED MOS_BITS)
    message(FATAL_ERROR "MOS_COMPILER_PREFIX or MOS_BITS is not defined, the target architecture config file is not correct")
endif()

mos_target_setup()

mos_kconfig(KERNEL      MOS_BITS                        ${MOS_BITS}         "ISA Bits")
mos_kconfig(LIMITS      MOS_MAX_CPU_COUNT               16                  "Max supported number of CPUs")
mos_kconfig(LIMITS      MOS_MAX_CMDLINE_COUNT           64                  "Max supported number of command line arguments")
mos_kconfig(LIMITS      MOS_PMM_EARLY_MEMREGIONS        64                  "Pre-allocated PMM list nodes")
mos_kconfig(LIMITS      MOS_PROCESS_MAX_OPEN_FILES      256                 "Max open files per process")
mos_kconfig(LIMITS      MOS_PROCESS_MAX_THREADS         64                  "Max threads per process")
mos_kconfig(LIMITS      MOS_PATH_MAX_LENGTH             256                 "Max length of a path")
mos_kconfig(LIMITS      MOS_STACK_PAGES_KERNEL          16                  "Pages of kernel stack")
mos_kconfig(LIMITS      MOS_STACK_PAGES_USER            32                  "Pages of user stack")
mos_kconfig(MM          MOS_MM_LIBALLOC_LOCKS           ON                  "Enable locking support in liballoc")
mos_kconfig(DEBUG       MOS_DEBUG_HAS_FUNCTION_NAME     ON                  "Include function name in debug log")

mos_kconfig(KERNEL      MOS_SMP                         OFF                 "Enable Experimental SMP support")

# Debugging options
mos_debug(thread        "Thread debug log")
mos_debug(process       "Process debug log")
mos_debug(fork          "Fork debug log")
mos_debug(scheduler     "Scheduler debug log")
mos_debug(syscall       "Syscall debug log")
mos_debug(pmm           "Physical memory manager debug log")
mos_debug(pmm_impl      "PMM internal implementation debug log")
mos_debug(vmm           "Virtual memory manager debug log")
mos_debug(vmm_impl      "VMM internal implementation debug log")
mos_debug(mmap          "Memory mapping debug log")
mos_debug(shm           "Shared memory debug log")
mos_debug(cow           "Copy-on-write debug log")
mos_debug(liballoc      "liballoc debug log")
mos_debug(elf           "ELF loader debug log")
mos_debug(fs            "Filesystem debug log")
mos_debug(io            "I/O debug log")
mos_debug(cpio          "CPIO debug log")
mos_debug(setup         "__setup debug log")
mos_debug(spinlock      "Spinlock debug log")
mos_debug(futex         "Futex debug log")
mos_debug(ipc           "IPC debug log")
mos_debug(vfs           "VFS debug log")
mos_debug(dcache        "DCache debug log")
mos_debug(dcache_ref    "DCache reference debug log")
mos_debug(tmpfs         "tmpfs debug log")
mos_debug(ipi           "IPI debug log")
