# SPDX-License-Identifier: GPL-3.0-or-later

include(add_nasm_binary)
include(prepare_bootable_kernel_binary)

mos_add_summary_section(ARCH_X86 "x86 Architecture Specifics")

if (NOT MOS_X86_HEAP_BASE_VADDR)
    set(MOS_X86_HEAP_BASE_VADDR 0xD0000000)
endif()

mos_add_summary_item(ARCH_X86 "x86 Heap Address" "${MOS_X86_HEAP_BASE_VADDR}")
mos_add_kconfig_define(MOS_X86_HEAP_BASE_VADDR)

add_bootable_target(boot/multiboot)
add_bootable_target(boot/multiboot_iso)

add_kernel_source(
    RELATIVE_SOURCES
        x86_platform.c
        x86_interrupt.c
        init/gdt.c
        init/idt.c
        init/descriptor_flush.asm
        init/interrupt_handler.asm
        drivers/text_mode_console.c
        drivers/port.c
        drivers/serial.c
        drivers/serial_console.c
        interrupt/pic.c
        interrupt/apic.c
        acpi/acpi.c
        cpu/cpu.c
        cpu/cpuid.c
        cpu/smp.c
        mm/paging.c
        mm/mm.c
        mm/enable_paging.asm
)
