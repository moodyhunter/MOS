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
    OUTPUT_VARIABLE MOS_KERNEL_REVISION_STRING
    ERROR_VARIABLE _DROP_
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_STRIP_TRAILING_WHITESPACE
)

mos_kconfig(PLATFORM    MOS_ISA_FAMILY              "ISA Family"                        "x86")
mos_kconfig(PLATFORM    MOS_ISA                     "ISA Name"                          "i686")
mos_kconfig(PLATFORM    MOS_BITS                    "ISA Bits"                          32)
mos_kconfig(PLATFORM    MOS_PAGE_SIZE               "Platform Page Size"                4096)
mos_kconfig(HARDWARE    MOS_MAX_CPU_COUNT           "Max Supported CPUs"                16)
mos_kconfig(LIMITS      MOS_PROCESS_MAX_OPEN_FILES  "Max Open Files per Process"        1024)
mos_kconfig(LIMITS      MOS_PROCESS_MAX_THREADS     "Max Threads per Process"           64)
mos_kconfig(LIMITS      MOS_PATH_MAX_LENGTH         "Max Path Length"                   256)
mos_kconfig(LIMITS      MOS_STACK_PAGES_KERNEL      "Size of Kernel Stack (pages)"      16)
mos_kconfig(LIMITS      MOS_STACK_PAGES_USER        "Size of User Stack (pages)"        32)
mos_kconfig(ADDRESS     MOS_USERSPACE_PGALLOC_START "Userspace Page Start"              0x40000000)
mos_kconfig(KERNEL      MOS_MM_LIBALLOC_LOCKS       "liballoc: has lock support"        OFF)
mos_kconfig(KERNEL      MOS_KERNEL_REVISION_STRING  "Kernel revision"                   ${MOS_KERNEL_REVISION_STRING})
mos_kconfig(DEBUG       MOS_MM_LIBALLOC_DEBUG       "liballoc: debug message enabled"   OFF)
mos_kconfig(DEBUG       MOS_DEBUG                   "Enable debug log"                  OFF)
mos_kconfig(DEBUG       MOS_PRINTK_HAS_FILENAME     "printk: has filename"              ON)
mos_kconfig(MISC        MOS_MEME                    "Enable MEME"                       OFF)
mos_kconfig(TESTS       BUILD_TESTING               "Build tests"                       OFF)

# x86 specific options
summary_section(ARCH_X86 "x86 Architecture")
mos_kconfig(ARCH_X86    MOS_X86_HEAP_BASE_VADDR     "Heap Address"                      0xD0000000)
mos_kconfig(ARCH_X86    MOS_X86_INITRD_VADDR        "Initrd Virtual Address"            0xC8000000)
