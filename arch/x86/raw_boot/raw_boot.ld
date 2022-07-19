/* SPDX-License-Identifier: GPL-3.0-or-later */

ENTRY(call_kernel_entry)
OUTPUT_FORMAT("binary")

KERNEL_BINARY_OFFSET = 0x1000;

SECTIONS
{
    . = KERNEL_BINARY_OFFSET;
    .text :
    {
        *(.kernel_entry_asm) /* defined in kernel_entry.asm */
        *(.text)
    }
    .rodata : { *(.rodata) }
    .data : { *(.data) }

    .bss : { *(.bss) }
}
