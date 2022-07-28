# SPDX-License-Identifier: GPL-3.0-or-later

include(add_nasm_binary)
include(prepare_bootable_kernel_binary)

add_bootable_target(boot/multiboot)
add_bootable_target(boot/multiboot_iso)

add_kernel_source(
    RELATIVE_SOURCES
        init/gdt.c
        init/gdt_flush.asm
        init/idt.c
        init/idt.asm
        drivers/screen.c
        drivers/port.c
)

