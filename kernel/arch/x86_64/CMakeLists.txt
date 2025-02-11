# SPDX-License-Identifier: GPL-3.0-or-later

add_kernel_source(
    interrupt/interrupt64.asm
    tasks/context_switch64.asm
    descriptors/flush64.asm
    acpi/acpi.cpp
    acpi/madt.cpp
    cpu/ap_entry.cpp
    cpu/cpu.cpp
    devices/rtc.cpp
    descriptors/descriptors.cpp
    interrupt/lapic.cpp
    interrupt/ioapic.cpp
    interrupt/idt.cpp
    interrupt/x86_interrupt.cpp
    interrupt/pic.cpp
    mm/paging.cpp
    tasks/context.cpp
    tasks/fpu_context.cpp
    x86_platform.cpp
    x86_platform_api.cpp
)
