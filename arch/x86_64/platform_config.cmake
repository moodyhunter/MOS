# SPDX-License-Identifier: GPL-3.0-or-later

set(MOS_COMPILER_PREFIX "x86_64-elf-")
set(MOS_CX_FLAGS "${MOS_CX_FLAGS} -mcmodel=large")
set(MOS_BITS 64)

add_summary_section(ARCH_X86_64 "x86_64 Architecture")
mos_kconfig(ARCH_X86_64    MOS_PAGE_SIZE                   4096                "x86_64 Page Size")
mos_kconfig(ARCH_X86_64    MOS_ADDR_USER_HEAP              0x0000000040000000  "User Heap Start Address")
mos_kconfig(ARCH_X86_64    MOS_ADDR_USER_STACK             0x0000000100000000  "User Stack Start Address")
mos_kconfig(ARCH_X86_64    MOS_ADDR_USER_MMAP              0x0000000C00000000  "User MMAP Start Address")
mos_kconfig(ARCH_X86_64    MOS_KERNEL_START_VADDR          0xC000000000000000  "Kernel Start Virtual Address")
mos_kconfig(ARCH_X86_64    MOS_ADDR_KERNEL_HEAP            0xFFFFFFFF00000000  "Kernel Heap Start Address")
mos_kconfig(ARCH_X86_64    MOS_ADDR_KERNEL_HEAP_END        0xFFFFFFFF00000000  "Kernel Heap Start Address")
mos_kconfig(ARCH_X86_64    MOS_PHYFRAME_ARRAY_VADDR        0xFA000000          "Physical Frame Array Virtual Address")
mos_kconfig(ARCH_X86_64    MOS_BIOS_VMAP_ADDR              0xFC000000          "BIOS Virtual Map Address")
