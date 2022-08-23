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
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include
    RELATIVE_SOURCES
        cpu/ap_init.asm
        interrupt/interrupt.asm
        mm/enable_paging.asm
        tasks/usermode.asm
        x86_descriptor_flush.asm

        acpi/acpi.c
        cpu/cpu.c
        cpu/cpuid.c
        cpu/smp.c
        devices/initrd_blockdev.c
        drivers/port.c
        drivers/serial.c
        drivers/serial_console.c
        drivers/text_mode_console.c
        gdt/gdt.c
        interrupt/apic.c
        interrupt/idt.c
        interrupt/interrupt.c
        interrupt/pic.c
        mm/mm.c
        mm/paging.c
        tasks/tss.c
        x86_platform.c
)
