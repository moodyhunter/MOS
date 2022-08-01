# SPDX-License-Identifier: GPL-3.0-or-later

include(add_nasm_binary)
include(prepare_bootable_kernel_binary)

add_bootable_target(boot/multiboot)
add_bootable_target(boot/multiboot_iso)

add_kernel_source(
    RELATIVE_SOURCES
        x86_platform.c
        x86_interrupt.c
        init/gdt.c
        init/idt.c
        init/tss.c
        init/gdt_tss_idt_flush.asm
        init/interrupt_handler.asm
        drivers/text_mode_console.c
        drivers/port.c
        drivers/serial.c
        drivers/serial_console.c
)
