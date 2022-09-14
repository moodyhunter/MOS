/* SPDX-License-Identifier: GPL-3.0-or-later */

ENTRY(mos_x86_multiboot_start)

SECTIONS
{
    /* !! Source: https://os.phil-opp.com/multiboot-kernel/#Linker%201M */
    /* ! We don't want to load the kernel to e.g. 0x0 because there are many special */
    /* ! memory areas below the 1MB mark (for example the so-called VGA buffer at 0xb8000 */
    /* ! that we use to print OK to the screen). */
    . = 1M; /* !! Lower Half */

    __MOS_STARTUP_START = .;
    .startup ALIGN(4K) :
    {
        *(.multiboot.data)
        *(.multiboot.text);
        *(.mos.startup.text);
        *(.mos.startup.data);
        . = ALIGN(4K);
        __MOS_STARTUP_PGD = .;
        . += 4K;
        __MOS_STARTUP_PGTABLE_0 = .;
        . += 4K;
        /* stack bottom */
        . += 16K;
        __MOS_STARTUP_STACK_TOP = .;
    }
    __MOS_STARTUP_END = .;

	/* ! The kernel will live at 3GB + 1MB in the virtual address space, */
	/* ! which will be mapped to 1MB in the physical address space. */
	/* ! Note that we page-align the sections. */
    . += 0xC0000000;

    /* !  : AT (ADDR(.text) - 0xC0000000) places the code themselves at a low address, but memory refs at the higher half */

    .text ALIGN(4K) : AT (ADDR(.text) - 0xC0000000)
    {
        __MOS_KERNEL_CODE_START = .;
        *(.text);
        __MOS_TEST_CASES_START = .;
        *(.mos.test_cases);
        __MOS_TEST_CASES_END = .;
        __MOS_KERNEL_CODE_END = .;
    }

    .rodata ALIGN(4K) : AT (ADDR(.rodata) - 0xC0000000)
    {
        __MOS_KERNEL_RO_START = .;
        *(.rodata);
        __MOS_KERNEL_RO_END = .;
    }

    .data ALIGN(4K) : AT (ADDR(.data) - 0xC0000000)
    {
        __MOS_KERNEL_RW_START = .;
        *(.data);
    }
    .bss ALIGN(4K) : AT (ADDR(.bss) - 0xC0000000) { *(.bss) }

    /* ! Without PAE, a maximum of 4G memory would take (1024 * 1024 * sizeof(PTE) + 1024 * sizeof(PDE)) */
    /* ! So that's a "TINY" 4KB larger than 4M */
    . = ALIGN(4K);
    __MOS_X86_PAGING_AREA_START = .;
    . += 4K;
    . = ALIGN(4K);
    . += 4M;
    . += 1M;    /* !! FIXME: for the stupid page map !! */
    __MOS_X86_PAGING_AREA_END = .;
    __MOS_KERNEL_RW_END = .;
    __MOS_KERNEL_END = .;
}
