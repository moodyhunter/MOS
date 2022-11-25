# SPDX-License-Identifier: GPL-3.0-or-later
summary_section(PLATFORM    "Platform")
summary_section(HARDWARE    "Hardware Support")
summary_section(LIMITS      "Limits")
summary_section(ADDRESS     "Special Addresses")
summary_section(DEBUG       "Debugging")
summary_section(MISC        "Miscellaneous")
summary_section(KERNEL      "Kernel Features")
summary_section(TESTS       "Tests")

execute_process(
    COMMAND git describe --long --tags --all --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE REVISION
    ERROR_VARIABLE _DROP_
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

mos_target_setup(x86 i686 32)

mos_kconfig(PLATFORM    MOS_PAGE_SIZE                   4096                "Platform page size")
mos_kconfig(HARDWARE    MOS_MAX_CPU_COUNT               16                  "Max supported number of CPUs")
mos_kconfig(LIMITS      MOS_PROCESS_MAX_OPEN_FILES      1024                "Max open files per process")
mos_kconfig(LIMITS      MOS_PROCESS_MAX_THREADS         64                  "Max threads per process")
mos_kconfig(LIMITS      MOS_PATH_MAX_LENGTH             256                 "Max length of a psath")
mos_kconfig(LIMITS      MOS_STACK_PAGES_KERNEL          16                  "Size of Kernel Stack (in pages)")
mos_kconfig(LIMITS      MOS_STACK_PAGES_USER            32                  "Size of User Stack (in pages)")
mos_kconfig(ADDRESS     MOS_USERSPACE_PGALLOC_START     0x40000000          "Userspace Page Start")
mos_kconfig(KERNEL      MOS_KERNEL_REVISION_STRING      ${REVISION}         "Kernel revision string")
mos_kconfig(KERNEL      MOS_MM_LIBALLOC_LOCKS           ON                  "Enable locking support in liballoc")
mos_kconfig(DEBUG       MOS_MM_LIBALLOC_DEBUG           OFF                 "Enable debug log for liballoc")
mos_kconfig(DEBUG       MOS_DEBUG                       OFF                 "Enable debug log")
mos_kconfig(DEBUG       MOS_VERBOSE_PRINTK              ON                  "Add filename and line number to printk")
mos_kconfig(MISC        MOS_MEME                        OFF                 "Enable MEME")
mos_kconfig(TESTS       BUILD_TESTING                   OFF                 "Kernel Space Unit Tests")

# x86 specific options
summary_section(ARCH_X86 "x86 Architecture")
mos_kconfig(ARCH_X86    MOS_X86_HEAP_BASE_VADDR         0xD0000000          "Heap Address")
mos_kconfig(ARCH_X86    MOS_X86_INITRD_VADDR            0xC8000000          "Initrd Virtual Address")
