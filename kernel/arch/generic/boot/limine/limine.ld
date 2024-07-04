/* SPDX-License-Identifier: GPL-3.0-or-later */

ENTRY(limine_entry)

SECTIONS
{
    . = 0xFFFFFFFF80000000;

    .text ALIGN(4K) :
    {
        __MOS_KERNEL_CODE_START = .;
        *(.text);
        *(.text.*);
        __MOS_KERNEL_CODE_END = .;
    }

    . = DATA_SEGMENT_ALIGN (CONSTANT (MAXPAGESIZE), CONSTANT (COMMONPAGESIZE));

    .rodata ALIGN(4K) :
    {
        __MOS_KERNEL_RODATA_START = .;
        *(.rodata);
        *(.srodata);
        *(.rodata.*);
        __MOS_KERNEL_RODATA_END = .;
    }

    .mos_info ALIGN(4K) :
    {
        __MOS_KERNEL_RW_START = .;
        __MOS_TEST_CASES_START = .;
        *(.mos.test_cases);
        __MOS_TEST_CASES_END = .;

        . = ALIGN(64);

        __MOS_EARLY_SETUP_START = .;
        *(.mos.early_setup);
        __MOS_EARLY_SETUP_END = .;

        . = ALIGN(64);

        __MOS_SETUP_START = .;
        *(.mos.setup);
        __MOS_SETUP_END = .;

        . = ALIGN(64);

        __MOS_INIT_START = .;
        *(.mos.init);
        __MOS_INIT_END = .;

        . = ALIGN(64);

        __MOS_PANIC_LIST_START = .;
        *(.mos.panic_list);
        __MOS_PANIC_LIST_END = .;

        . = ALIGN(64);

        __MOS_PANIC_HOOKS_START = .;
        *(.mos.panic_hooks);
        __MOS_PANIC_HOOKS_END = .;

        . = ALIGN(64);

        __MOS_SCHEDULERS_START = .;
        *(.mos.schedulers);
        __MOS_SCHEDULERS_END = .;
    }

    .data ALIGN(4K) :
    {
        *(.data);
        *(.sdata);
        /* limine bootloader data */
        KEEP(*(.limine.markers.requests_start))
        KEEP(*(.limine.requests))
        KEEP(*(.limine.markers.requests_end))
    }

    .init_array ALIGN(4K) :
    {
        PROVIDE_HIDDEN (__init_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
        KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
        PROVIDE_HIDDEN (__init_array_end = .);
    }

    .fini_array ALIGN(4K) :
    {
        PROVIDE_HIDDEN (__fini_array_start = .);
        KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
        KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
        PROVIDE_HIDDEN (__fini_array_end = .);
    }

    .bss ALIGN(4K) :
    {
        *(.bss);
        *(.sbss);
        *(.bss.*);
        __MOS_KERNEL_RW_END = .;
        __MOS_KERNEL_END = .;
    }
}
