[bits 32]
global x86_enable_paging_impl

x86_enable_paging_impl:
    push ebp
    mov ebp, esp

    mov eax, [esp + 8]
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80010000      ; enable paging
    mov cr0, eax
	mov	esp, ebp
	pop	ebp
	ret

