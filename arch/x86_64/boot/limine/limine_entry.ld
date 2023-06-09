/* SPDX-License-Identifier: GPL-3.0-or-later */

ENTRY(mos_limine_entry)

SECTIONS
{
    . = 0xC000000000000000;

    /* ! AT (ADDR(.text) - 0xf000000000000000) places the code themselves at a low address, but memory refs at the higher half */

    .text ALIGN(4K) : AT (ADDR(.text) - 0xC000000000000000)
    {
        __MOS_KERNEL_CODE_START = .;
        *(.text);
        *(.text.*);
        __MOS_KERNEL_CODE_END = .;
    }

    . = DATA_SEGMENT_ALIGN (CONSTANT (MAXPAGESIZE), CONSTANT (COMMONPAGESIZE));

    .rodata ALIGN(4K) : AT (ADDR(.rodata) - 0xC000000000000000)
    {
        __MOS_KERNEL_RODATA_START = .;
        *(.rodata);
        *(.rodata.*);

        . = ALIGN(4K);
        __MOS_TEST_CASES_START = .;
        *(.mos.test_cases);
        __MOS_TEST_CASES_END = .;
    }

    .mos_info ALIGN(4K) : AT (ADDR(.mos_info) - 0xC0000000)
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

    .data ALIGN(4K) : AT (ADDR(.data) - 0xC000000000000000)
    {
        __MOS_KERNEL_RW_START = .;
        *(.data);
    }

    .bss ALIGN(4K) : AT (ADDR(.bss) - 0xC000000000000000)
    {
        *(.bss);
        . = ALIGN(4K);
        . += 1M * 16; /* ! 16 MiB for all CPUs' stacks (defined in mos_config.cmake) */
        __MOS_KERNEL_HIGHER_STACK_TOP = .;
        __MOS_KERNEL_RW_END = .;
        __MOS_KERNEL_END = .;
    }
}
