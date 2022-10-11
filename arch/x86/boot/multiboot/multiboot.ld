/* SPDX-License-Identifier: GPL-3.0-or-later */

ENTRY(mos_x86_multiboot_start)

SECTIONS
{
    /* !! Source: https://os.phil-opp.com/multiboot-kernel/#Linker%201M */
    /* ! We don't want to load the kernel to e.g. 0 because there are many special memory areas below the 1MB mark */
    /* ! (for example the so-called VGA buffer at 0xb8000. */
    . = 5M; /* !! Lower Half */

    _mos_startup_START = .;
    .startup ALIGN(4K) :
    {
        *(.multiboot.data); /* ! Multiboot header */
        *(.multiboot.text);
        *(.mos.startup.text);
        *(.mos.startup.rodata);
    }

    .startup.rw ALIGN(4K) :
    {
        *(.mos.startup.data);
        . = ALIGN(4K);
        _mos_startup_PGD = .;
        . += 4K;
        _mos_startup_PGTABLE = .;
        . += 512K;  /* ! 512 KiB paging area (aka 512MiB memory) */
        . += 4K;
        _mos_startup_stack = .;
    }
    _mos_startup_END = .;

    . += 0xC0000000;

    /* ! AT (ADDR(.text) - 0xC0000000) places the code themselves at a low address, but memory refs at the higher half */

    .text ALIGN(4K) : AT (ADDR(.text) - 0xC0000000)
    {
        __MOS_KERNEL_CODE_START = .;
        *(.text);
        . = ALIGN(4K);
        __MOS_TEST_CASES_START = .;
        *(.mos.test_cases);
        __MOS_TEST_CASES_END = .;
        __MOS_KERNEL_CODE_END = .;
    }

    . = DATA_SEGMENT_ALIGN (CONSTANT (MAXPAGESIZE), CONSTANT (COMMONPAGESIZE));

    .rodata ALIGN(4K) : AT (ADDR(.rodata) - 0xC0000000)
    {
        __MOS_KERNEL_RODATA_START = .;
        *(.rodata);
        *(.rodata.*);
        __MOS_KERNEL_RODATA_END = .;
    }

    .data ALIGN(4K) : AT (ADDR(.data) - 0xC0000000)
    {
        __MOS_KERNEL_RW_START = .;
        *(.data);

        /* ! Without PAE, a maximum of 4G memory would take (1024 * 1024 * sizeof(PTE) + 1024 * sizeof(PDE)) */
        . = ALIGN(4K);
        __MOS_X86_PMEM_FREE_LIST = .;
        . += 6M;    /* ! for pmem_freelist */
        __MOS_X86_PAGING_AREA_START = .;
        . += 4K;
        . += 4M;
        . += 1M;    /* ! for the stupid page map */
        __MOS_X86_PAGING_AREA_END = .;
    }

    .bss ALIGN(4K) : AT (ADDR(.bss) - 0xC0000000)
    {
        *(.bss);
        . = ALIGN(4K);
        . += 16K;  /* ! stack */
        __MOS_KERNEL_HIGHER_STACK_TOP = .;
    }
    __MOS_KERNEL_RW_END = .;
    __MOS_KERNEL_END = .;
}
