# SPDX-License-Identifier: GPL-3.0-or-later

# ! This file contains the default configuration for MOS.
# ! To override the default values, use CMake's -D option.

summary_section(HARDWARE    "Hardware Support")
summary_section(LIMITS      "Limits")
summary_section(DEBUG       "Debugging")
summary_section(MISC        "Miscellaneous")
summary_section(KERNEL      "Kernel Features")
summary_section(TESTS       "Tests")
summary_section(MM          "Memory Management")

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

if(NOT DEFINED MOS_ARCH OR NOT EXISTS ${_ARCH_CONFIGURATION_FILE})
    message(FATAL_ERROR "MOS_ARCH is not defined or is incorrect, please specify the target architecture")
endif()

include(${_ARCH_CONFIGURATION_FILE})

if(NOT DEFINED MOS_COMPILER_PREFIX OR NOT DEFINED MOS_BITS)
    message(FATAL_ERROR "MOS_COMPILER_PREFIX or MOS_BITS is not defined, the target architecture config file is not correct")
endif()

mos_target_setup()

mos_kconfig(KERNEL      MOS_BITS                        ${MOS_BITS}         "ISA Bits")
mos_kconfig(LIMITS      MOS_MAX_CPU_COUNT               16                  "Max supported number of CPUs")
mos_kconfig(LIMITS      MOS_MAX_SUPPORTED_MEMREGION     32                  "Max supported memory regions")
mos_kconfig(LIMITS      MOS_PROCESS_MAX_OPEN_FILES      1024                "Max open files per process")
mos_kconfig(LIMITS      MOS_PROCESS_MAX_THREADS         64                  "Max threads per process")
mos_kconfig(LIMITS      MOS_PATH_MAX_LENGTH             256                 "Max length of a psath")
mos_kconfig(LIMITS      MOS_STACK_PAGES_KERNEL          16                  "Pages of kernel stack")
mos_kconfig(LIMITS      MOS_STACK_PAGES_USER            32                  "Pages of user stack")

mos_kconfig(MM          MOS_MM_LIBALLOC_LOCKS           ON                  "Enable locking support in liballoc")
mos_kconfig(DEBUG       MOS_PRINTK_WITH_FILENAME        OFF                 "Print filename in printk")
mos_kconfig(TESTS       BUILD_TESTING                   OFF                 "Kernel Unit Tests")

# Debugging options
mos_debug(thread        "Thread debug log")
mos_debug(process       "Process debug log")
mos_debug(fork          "Fork debug log")
mos_debug(scheduler     "Scheduler debug log")
mos_debug(syscall       "Syscall debug log")
mos_debug(pmm           "Physical memory manager debug log")
mos_debug(vmm           "Virtual memory manager debug log")
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
