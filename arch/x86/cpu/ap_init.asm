; SPDX-License-Identifier: GPL-3.0-or-later

[global x86_ap_trampoline]

[extern ap_state]
[extern ap_stack_addr]
[extern ap_pgd_addr]
[extern ap_begin_exec]

%define X86_AP_TRAMPOLINE_ADDR      0x8000
%define X86_KERNEL_VADDR            0xc0000000
%define AP_STATUS_BSP_STARTUP_SENT  1
%define AP_STATUS_START_REQUEST     2
%define AP_STATUS_START             3

begin:
x86_ap_trampoline:
[bits 16]
    cli
    cld
    lgdt    [gdt_ptr - begin + X86_AP_TRAMPOLINE_ADDR]
    mov     eax, cr0
    or      eax, 0x1
    mov     cr0, eax
    jmp     0x08:(pm_init - begin + X86_AP_TRAMPOLINE_ADDR)

pm_init:
[bits 32]
    mov     bx, 0x10            ; Data Segment
    mov     ds, bx
    mov     es, bx
    mov     ss, bx
    mov     fs, bx
    mov     gs, bx

spin_wait_for_bsp_startup:
    pause
    cmp     dword [ap_state - X86_KERNEL_VADDR], AP_STATUS_BSP_STARTUP_SENT
    jne     spin_wait_for_bsp_startup
spin_wait_for_bsp_startup.end:

    ; enable paging and global pages
    mov     eax, dword [ap_pgd_addr - X86_KERNEL_VADDR]
    mov     cr3, eax

    ; enable paging
    mov     eax, cr0
    or      eax, 0x80010000
    mov     cr0, eax

    ; enable cache
    mov     eax, cr0
    and     eax, 0xfffeffff
    mov     cr0, eax

    ; enable PGE
    mov     eax, cr4
    or      eax, 0x00000080
    mov     cr4, eax

    mov     esp, dword [ap_stack_addr]
    mov     ebp, 0

    mov     dword [ap_state], AP_STATUS_START_REQUEST
spin_wait_for_start:
    pause
    cmp     dword [ap_state], AP_STATUS_START
    jne     spin_wait_for_start
spin_wait_for_start.end:

    call    0x08:ap_begin_exec
    cli
    hlt


;! https://wiki.osdev.org/SMP says that the data segment is "0x008F9200"
;! this is wrong, it should be "C" instead of "8"
;* (See GDT -> Segment Descriptors -> Flags -> DB)

align   16
tmp_gdt:
    dd      0, 0
    dd      0x0000FFFF, 0x00CF9A00 ; Code
    dd      0x0000FFFF, 0x00CF9200 ; Data
.end:

align   16
gdt_ptr:
    dw      tmp_gdt.end - tmp_gdt - 1
    dd      tmp_gdt - begin + X86_AP_TRAMPOLINE_ADDR
