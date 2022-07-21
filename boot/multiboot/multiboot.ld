/* SPDX-License-Identifier: GPL-3.0-or-later */

ENTRY(_start)

KERNEL_BINARY_OFFSET = 0x1000;

SECTIONS
{
    . = KERNEL_BINARY_OFFSET;
	.multiboot.data : { *(.multiboot.data) }
	.multiboot.text : { *(.multiboot.text) }
    .text : { *(.text) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(.bss) }
}
