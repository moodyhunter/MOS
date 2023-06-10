# SPDX-License-Identifier: GPL-3.0-or-later

set(MOS_COMPILER_PREFIX "i686-elf-")
set(MOS_BITS 32)

add_summary_section(ARCH_X86 "x86 Architecture")
add_summary_section(ARCH_X86_DEBUG "x86 Debug")

mos_kconfig(ARCH_X86    MOS_PAGE_SIZE                   4096                "x86 Page Size")
mos_kconfig(ARCH_X86    MOS_ADDR_USER_HEAP              0x40000000          "User Heap Start Address")
mos_kconfig(ARCH_X86    MOS_ADDR_USER_STACK             0x60000000          "User Stack Start Address")
mos_kconfig(ARCH_X86    MOS_ADDR_USER_MMAP              0x90000000          "User MMAP Start Address")
mos_kconfig(ARCH_X86    MOS_ADDR_KERNEL_HEAP            0xC5000000          "Kernel Heap Start Address")
mos_kconfig(ARCH_X86    MOS_ADDR_KERNEL_HEAP_END        0xF8000000          "Kernel Heap End Address")
mos_kconfig(ARCH_X86    MOS_INITRD_VADDR                0xF8000000          "Initrd Virtual Address")
mos_kconfig(ARCH_X86    MOS_PHYFRAME_ARRAY_VADDR        0xFA000000          "Physical Frame Array Virtual Address")
mos_kconfig(ARCH_X86    MOS_BIOS_VMAP_ADDR              0xFC000000          "BIOS Virtual Map Address")
mos_kconfig(ARCH_X86    MOS_X86_INITIAL_STACK_SIZE      0x100000            "Initial Stack Size")   # Update corresponding linker script if changed
mos_kconfig(ARCH_X86    MOS_KERNEL_START_VADDR          0xC0000000          "Kernel start virtual address")

mos_kconfig(ARCH_X86_DEBUG X86_DEBUG_ALL OFF "Enable all x86 debug logs")

macro(x86_debug feature description)
    mos_kconfig(ARCH_X86_DEBUG MOS_DEBUG_x86_${feature} ${X86_DEBUG_ALL} ${description})
endmacro()

x86_debug(cpu       "x86 CPU debug log")
x86_debug(lapic     "x86 Local APIC debug log")
x86_debug(ioapic    "x86 I/O APIC debug log")
x86_debug(startup   "x86 Startup debug log")
x86_debug(acpi      "x86 ACPI debug log")
