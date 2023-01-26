# SPDX-License-Identifier: GPL-3.0-or-later

set(MOS_COMPILER_PREFIX "x86_64-elf-")
set(MOS_BITS 64)

summary_section(ARCH_X86_64 "x86 Architecture")
mos_kconfig(ARCH_X86_64    MOS_PAGE_SIZE                   4096                "x86_64 Page Size")
mos_kconfig(ARCH_X86_64    MOS_ADDR_USER_HEAP              0x0000000040000000  "User Heap Start Address")
mos_kconfig(ARCH_X86_64    MOS_ADDR_USER_STACK             0x0000000100000000  "User Stack Start Address")
mos_kconfig(ARCH_X86_64    MOS_ADDR_USER_MMAP              0x0000000C00000000  "User MMAP Start Address")
mos_kconfig(ARCH_X86_64    MOS_ADDR_KERNEL_HEAP            0xFFFFFFFF00000000  "Kernel Heap Start Address")
