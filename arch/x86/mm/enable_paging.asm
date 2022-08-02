[bits 32]
global x86_enable_paging

x86_enable_paging:
    push ebp
    mov ebp, esp
    mov eax, [esp + 8]
    mov cr3, eax
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax
	mov	esp, ebp
	pop	ebp
	ret

