[bits 32]
global x86_enable_paging_impl

x86_enable_paging_impl:
    mov eax, [esp + 4]

    ; page directory
    mov cr3, eax

    ; enable paging
    mov eax, cr0
    or  eax, 0x80010000
    mov cr0, eax

    ; enable PGE
    mov eax, cr4
    or  eax, 0x00000080
    mov cr4, eax
	ret
