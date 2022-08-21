; SPDX-License-Identifier: GPL-3.0-or-later

global x86_usermode_trampoline:function (x86_usermode_trampoline.end - x86_usermode_trampoline)

global jump_to_usermain:function (jump_to_usermain.end - jump_to_usermain)

extern main

x86_usermode_trampoline:
    .end:
jump_to_usermain:
    mov     eax, [esp + 4]
    mov     ebx, [esp + 8]

    mov     cx, 0x20 | 3    ; user data segment + ring 3
    mov     ds, cx
    mov     es, cx
    mov     fs, cx
    mov     gs, cx

    push    0x20 | 3        ; user data segment + ring 3
    push    ebx             ; esp
    pushf                   ; eflags
    push    0x18 | 3        ; user code segment + ring 3
    push    eax             ; eip
    iret
.end:

curr_offset equ 20
next_offset equ curr_offset + 4

global context_switch
; TCB_t *context_switch(TCB_t *cur, TCB_t *next);
context_switch:
    ; Save old callee-preserved registers
    ; No need to store eax-edx as they are handled
    ; by the interrupt  handler
    push  ebp
    push  ebx
    push  esi
    push  edi


    ; Store the current stack pointer in the TCB struct
    mov edx, 0 ; currently just have stack* at offset 0 in struct but this can change
    mov eax, [curr_offset+esp]
    mov [eax+edx*1],esp


    ; Load the stack pointer of the next Thread struct
    mov ecx, [next_offset+esp]
    mov esp, [ecx+edx*1]

    ; Load new callee-preserved registers
    pop  edi
    pop  esi
    pop  ebx
    pop  ebp

    ret
