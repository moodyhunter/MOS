/* SPDX-License-Identifier: GPL-3.0-or-later */

ENTRY(_mos_x86_multiboot_start)

SECTIONS
{
    /* !! Source: https://os.phil-opp.com/multiboot-kernel/#Linker%201M */
    /* ! We don't want to load the kernel to e.g. 0x0 because there are many special */
    /* ! memory areas below the 1MB mark (for example the so-called VGA buffer at 0xb8000 */
    /* ! that we use to print OK to the screen). */
    . = 5M; /* !! Lower Half Kernel */

    . =  ALIGN(0x1000);
    __MOS_KERNEL_RO_START = .;
    __MOS_SECTION_MULTIBOOT_START = .;
	.multiboot.data : { *(.multiboot.data) }
	.multiboot.text : { *(.multiboot.text) }
    __MOS_SECTION_MULTIBOOT_END = .;

    .text : {
        __MOS_KERNEL_TEXT_START = .;
         *(.text)
        __MOS_KERNEL_TEXT_END = .;
    }

    .rodata : {
        __MOS_KERNEL_RODATA_START = .;
        *(.rodata)
        __MOS_KERNEL_RODATA_END = .;
    }
    . = ALIGN(0x1000);
    __MOS_KERNEL_RO_END = .;
    __MOS_KERNEL_RW_START = .;
    .data : { *(.data) }
    .bss : { *(.bss) }

    /* ! Without PAE, a maximum of 4G memory would take (1024 * 1024 * sizeof(PTE) + 1024 * sizeof(PDE)) */
    /* ! So that's a "TINY" 4KB larger than 4M */
    . = ALIGN(0x1000);
    __MOS_X86_PAGING_AREA_START = .;
    . += 4K;
    . = ALIGN(0x1000);
    . += 4M;
    . += 1M;    /* !! FIXME: for the stupid page map !! */
    __MOS_X86_PAGING_AREA_END = .;
    __MOS_KERNEL_RW_END = .;

    __MOS_KERNEL_END = .;
}
