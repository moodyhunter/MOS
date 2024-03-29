/* SPDX-License-Identifier: GPL-3.0-or-later */

ENTRY(limine_entry)

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    /* ! AT (ADDR(.text) - 0xFFFFFFFF80000000) places the code themselves at a low address, but memory refs at the higher half */

    .text ALIGN(4K) : AT (ADDR(.text) - 0xFFFFFFFF80000000)
    {
        __MOS_KERNEL_CODE_START = .;
        *(.text);
        *(.text.*);
        __MOS_KERNEL_CODE_END = .;
    }

    . = DATA_SEGMENT_ALIGN (CONSTANT (MAXPAGESIZE), CONSTANT (COMMONPAGESIZE));

    .rodata ALIGN(4K) : AT (ADDR(.rodata) - 0xFFFFFFFF80000000)
    {
        __MOS_KERNEL_RODATA_START = .;
        *(.rodata);
        *(.rodata.*);
    }

    .limine_reqs ALIGN(4K) : AT (ADDR(.limine_reqs) - 0xFFFFFFFF80000000)
    {
        *(.limine_reqs);
    }

    .mos_info ALIGN(4K) : AT (ADDR(.mos_info) - 0xFFFFFFFF80000000)
    {
        __MOS_KERNEL_MOS_INFO_START = .;

        __MOS_TEST_CASES_START = .;
        *(.mos.test_cases);
        __MOS_TEST_CASES_END = .;

        __MOS_EARLY_SETUP_START = .;
        *(.mos.early_setup);
        __MOS_EARLY_SETUP_END = .;

        __MOS_SETUP_START = .;
        *(.mos.setup);
        __MOS_SETUP_END = .;

        __MOS_INIT_START = .;
        *(.mos.init);
        __MOS_INIT_END = .;

        __MOS_KERNEL_MOS_INFO_END = .;
        __MOS_KERNEL_RODATA_END = .;
    }

    .data ALIGN(4K) : AT (ADDR(.data) - 0xFFFFFFFF80000000)
    {
        __MOS_KERNEL_RW_START = .;
        *(.data);
    }

    .init_array ALIGN(4K) : AT (ADDR(.init_array) - 0xFFFFFFFF80000000)
    {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
        PROVIDE_HIDDEN (__init_array_end = .);
    }

    .fini_array ALIGN(4K) : AT (ADDR(.fini_array) - 0xFFFFFFFF80000000)
    {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
        PROVIDE_HIDDEN (__fini_array_end = .);
    }

    .bss ALIGN(4K) : AT (ADDR(.bss) - 0xFFFFFFFF80000000)
    {
        *(.bss);
        *(.bss.*);
        . = ALIGN(4K);
        . += 1M * 16; /* ! 16 MiB for all CPUs' stacks (defined in mos_config.cmake) */
        __MOS_KERNEL_HIGHER_STACK_TOP = .;
        __MOS_KERNEL_RW_END = .;
        __MOS_KERNEL_END = .;
    }
}
