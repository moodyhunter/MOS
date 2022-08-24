; SPDX-License-Identifier: GPL-3.0-or-later

[bits 16]
[global x86_ap_trampoline]

[extern ap_state]
[extern ap_stack_addr]
[extern ap_begin_exec]

x86_ap_trampoline_ADDR equ 0x8000

AP_STATUS_BSP_STARTUP_SENT          equ 1
AP_STATUS_AP_WAIT_FOR_STACK_ALLOC   equ 2
AP_STATUS_STACK_ALLOCATED           equ 3
AP_STATUS_STACK_INITIALIZED         equ 4
AP_STATUS_START                     equ 5
AP_STATUS_STARTED                   equ 6

%macro ap_wait 1
spin_for_status_%1:
    pause
    cmp     dword [ap_state], %1
    jne     spin_for_status_%1
%endmacro

x86_ap_trampoline:
    cli
    cld
    or      ax, ax              ; clear segment registers
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     fs, ax
    mov     gs, ax
    lgdt    [gdt_ptr - x86_ap_trampoline + x86_ap_trampoline_ADDR]
    mov     eax, cr0
    or      eax, 0x1
    mov     cr0, eax
    jmp     0x08:(pm_init - x86_ap_trampoline + x86_ap_trampoline_ADDR)

[bits 32]
pm_init:
    mov     bx, 0x10            ; Data Segment
    mov     ds, bx
    mov     es, bx
    mov     ss, bx
    push    0x0                 ; reset EFLAGS
    popfd

    ap_wait AP_STATUS_BSP_STARTUP_SENT
    mov     dword [ap_state], AP_STATUS_AP_WAIT_FOR_STACK_ALLOC

    ap_wait AP_STATUS_STACK_ALLOCATED
    mov     esp, dword [ap_stack_addr]
    mov     ebp, 0
    mov     dword [ap_state], AP_STATUS_STACK_INITIALIZED

    ap_wait AP_STATUS_START
    mov     dword [ap_state], AP_STATUS_STARTED
    call    0x08:ap_begin_exec
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
    dd      tmp_gdt - x86_ap_trampoline + x86_ap_trampoline_ADDR
