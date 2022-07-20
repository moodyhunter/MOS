/* SPDX-License-Identifier: GPL-3.0-or-later */

ENTRY(call_kernel_entry)
OUTPUT_FORMAT(binary)

KERNEL_BINARY_OFFSET = 0x1000;

SECTIONS
{
    . = KERNEL_BINARY_OFFSET;
    /* defined in kernel_entry.asm */
    .kernel_entry_asm : { *(.kernel_entry_asm) }
    .text : { *(.text) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(.bss) }
}
