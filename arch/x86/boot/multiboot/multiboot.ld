/* SPDX-License-Identifier: GPL-3.0-or-later */

ENTRY(_start)

SECTIONS
{
    /* !! Source: https://os.phil-opp.com/multiboot-kernel/#Linker%201M */

    /* ! We don't want to load the kernel to e.g. 0x0 because there are many special */
    /* ! memory areas below the 1MB mark (for example the so-called VGA buffer at 0xb8000 */
    /* ! that we use to print OK to the screen). */
    . = 1M;

    __MOS_SECTION_MULTIBOOT_START = .;
	.multiboot.data : { *(.multiboot.data) }
	.multiboot.text : { *(.multiboot.text) }
    __MOS_SECTION_MULTIBOOT_END = .;

    __MOS_SECTION_KERNEL_START = .;
    .text : { *(.text) }
    .rodata : { *(.rodata) }
    .data : { *(.data) }
    .bss : { *(.bss) }
    __MOS_SECTION_KERNEL_END = .;
}
