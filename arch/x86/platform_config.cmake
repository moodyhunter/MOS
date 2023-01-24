# SPDX-License-Identifier: GPL-3.0-or-later

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
