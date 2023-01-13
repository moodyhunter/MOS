# SPDX-License-Identifier: GPL-3.0-or-later

# ! This file contains the default configuration for MOS.
# ! To override the default values, use CMake's -D option.

summary_section(HARDWARE    "Hardware Support")
summary_section(LIMITS      "Limits")
summary_section(ADDR        "Special Addresses")
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

mos_target_setup(x86 i686 32)

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
mos_debug(schedule      "Scheduler debug log")
mos_debug(syscall       "Syscall debug log")
mos_debug(pmm           "Physical memory manager debug log")
mos_debug(vmm           "Virtual memory manager debug log")
mos_debug(mmap          "Memory mapping debug log")
mos_debug(cow           "Copy-on-write debug log")
mos_debug(liballoc      "liballoc debug log")
mos_debug(elf           "ELF loader debug log")
mos_debug(fs            "Filesystem debug log")
mos_debug(io            "I/O debug log")
mos_debug(cpio          "CPIO debug log")
mos_debug(init          "Init debug log")
mos_debug(spinlock      "Spinlock debug log")

# x86 specific options
summary_section(ARCH_X86 "x86 Architecture")
mos_kconfig(ARCH_X86    MOS_PAGE_SIZE                   4096                "x86 Page Size")
mos_kconfig(ARCH_X86    MOS_ADDR_USER_HEAP              0x40000000          "User Heap Start Address")
mos_kconfig(ARCH_X86    MOS_ADDR_USER_STACK             0x60000000          "User Stack Start Address")
mos_kconfig(ARCH_X86    MOS_ADDR_USER_MMAP              0x90000000          "User MMAP Start Address")
mos_kconfig(ARCH_X86    MOS_ADDR_KERNEL_HEAP            0xD0000000          "Kernel Heap Start Address")
mos_kconfig(ARCH_X86    MOS_X86_INITRD_VADDR            0xE0000000          "Initrd Virtual Address")
mos_kconfig(ARCH_X86    MOS_X86_INITIAL_STACK_SIZE      0x100000            "Initial Stack Size")   # Update corresponding linker script if changed

mos_debug(x86_paging    "x86 Paging debug log")
mos_debug(x86_cpu       "x86 CPU debug log")
mos_debug(x86_lapic     "x86 Local APIC debug log")
mos_debug(x86_ioapic    "x86 I/O APIC debug log")
