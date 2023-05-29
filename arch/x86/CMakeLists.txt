# SPDX-License-Identifier: GPL-3.0-or-later

add_bootable_target(boot/multiboot)
add_bootable_target(boot/multiboot_iso)

add_kernel_source(
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include/private
    SYSTEM_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include/public
    SOURCES
        interrupt/interrupt.asm
        mm/enable_paging.asm
        tasks/context_switch.asm
        x86_descriptor_flush.asm
        acpi/acpi.c
        acpi/madt.c
        cpu/cpuid.c
        cpu/ap_entry.c
        devices/initrd_blockdev.c
        devices/serial.c
        devices/serial_console.c
        descriptors/descriptors.c
        interrupt/lapic.c
        interrupt/ioapic.c
        interrupt/idt.c
        interrupt/x86_interrupt.c
        interrupt/pic.c
        mm/mm.c
        mm/paging.c
        mm/paging_impl.c
        tasks/context.c
        x86_platform.c
        x86_platform_api.c
)

if(MOS_SMP)
    add_kernel_source(
        SOURCES
            cpu/ap_init.asm
            cpu/smp.c
    )
endif()
