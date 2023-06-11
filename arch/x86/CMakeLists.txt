# SPDX-License-Identifier: GPL-3.0-or-later

if (NOT MOS_X86_64)
    # multiboot is only available on 32-bit x86
    add_subdirectory(boot/multiboot)
else()
    add_subdirectory(boot/limine)
endif()

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
        cpu/cpuid.c
        cpu/ap_entry.c
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
