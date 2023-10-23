# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(boot/limine)

add_kernel_source(
    INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include/private
    SYSTEM_INCLUDE_DIRECTORIES
        ${CMAKE_CURRENT_LIST_DIR}/include/public
    SOURCES
        interrupt/interrupt${MOS_BITS}.asm
        tasks/context_switch${MOS_BITS}.asm
        descriptors/flush${MOS_BITS}.asm
        acpi/acpi.c
        acpi/madt.c
        cpu/ap_entry.c
        devices/serial.c
        devices/serial_console.c
        devices/rtc.c
        descriptors/descriptors.c
        interrupt/lapic.c
        interrupt/ioapic.c
        interrupt/idt.c
        interrupt/x86_interrupt.c
        interrupt/pic.c
        mm/mm.c
        mm/paging.c
        tasks/context.c
        tasks/fpu_context.c
        x86_platform.c
        x86_platform_api.c
)
