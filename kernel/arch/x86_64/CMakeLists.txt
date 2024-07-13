# SPDX-License-Identifier: GPL-3.0-or-later

add_kernel_source(
    interrupt/interrupt64.asm
    tasks/context_switch64.asm
    descriptors/flush64.asm
    acpi/acpi.c
    acpi/madt.c
    cpu/ap_entry.c
    cpu/cpu.c
    devices/serial_driver.c
    devices/rtc.c
    descriptors/descriptors.c
    interrupt/lapic.c
    interrupt/ioapic.c
    interrupt/idt.c
    interrupt/x86_interrupt.c
    interrupt/pic.c
    mm/paging.c
    tasks/context.c
    tasks/fpu_context.c
    x86_platform.c
    x86_platform_api.c
)
