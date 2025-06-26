SECTIONS {
    .mos.modinfo : {
        KEEP(*(.mos.modinfo))
    }
    /DISCARD/ : {
        *(.discard)
        *(.discard.*)
        *(.export_symbol)
        *(.no_trim_symbol)
        *(.group)
        *(.debug_macro)
        *(.rela.debug_macro)
        *(.eh_frame)
    }
    .text : {
        *(.text)
        *(.text.*)
        *(.gnu.linkonce.t.*)
    }
    .rodata : {
        *(.rodata)
        *(.rodata.*)
        *(.gnu.linkonce.r.*)
    }
    .data : {
        *(.data)
        *(.data.*)
        *(.gnu.linkonce.d.*)
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
    .bss : {
        *(.bss)
        *(.bss.*)
        *(.gnu.linkonce.b.*)
    }
}
