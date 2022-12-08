# SPDX-License-Identifier: GPL-3.0-or-later

include(prepare_bootable_kernel_binary)
add_bootable_target(boot/multiboot)
add_bootable_target(boot/multiboot_iso)

add_kernel_source(
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include
    RELATIVE_SOURCES
        cpu/ap_init.asm
        interrupt/interrupt.asm
        mm/enable_paging.asm
        tasks/context_switch.asm
        x86_descriptor_flush.asm

        acpi/acpi.c
        cpu/cpuid.c
        cpu/smp.c
        devices/initrd_blockdev.c
        devices/serial.c
        devices/serial_console.c
        devices/text_mode_console.c
        gdt/gdt.c
        interrupt/lapic.c
        interrupt/idt.c
        interrupt/x86_interrupt.c
        interrupt/pic.c
        mm/mm.c
        mm/paging.c
        mm/paging_impl.c
        mm/pmem_freelist.c
        tasks/tss.c
        tasks/context.c
        x86_platform.c
        x86_platform_api.c
)
