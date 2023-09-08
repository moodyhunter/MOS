; SPDX-License-Identifier: GPL-3.0-or-later

%define REGSIZE 8

; void x86_context_switch_impl(ptr_t x86_context, ptr_t *old_stack, ptr_t kernel_stack, ptr_t jump_addr)
; RDI, RSI, RDX, RCX, R8, R9
global x86_context_switch_impl:function (x86_context_switch_impl.end - x86_context_switch_impl)
x86_context_switch_impl:
    push    rbp
    mov     rbp, rsp

    ; If the callee wishes to use registers RBX, RSP, RBP, and R12-R15,
    ; it must restore their original values before returning control to the caller.
    ; All other registers must be saved by the caller if it wishes to preserve their values.

    ; save callee-saved registers
    pushf
    push    rbx
    push    r12
    push    r13
    push    r14
    push    r15

    ; rdi = x86_context
    ; rsi = *old_stack
    ; rdx = kernel_stack
    ; rcx = jump_addr
    ; set rsp to kernel_stack
    mov     [rsi], rsp      ; backup old stack pointer
    mov     rsp, rdx        ; switch to kernel stack

    xor     rax, rax        ; clear rax, rbx, rsi, rdi, rbp
    xor     rbx, rbx
    xor     rdx, rdx
    xor     rsi, rsi
    xor     rbp, rbp
    ; rcx contains jump_addr
    ; rdi contains arguments for the new thread, don't clear it
    ; rdi = struct { x86_stack_frame, arg };
    ; jump to jump_addr
    jmp     rcx
.end:

global x86_normal_switch:function (x86_normal_switch.end - x86_normal_switch)
x86_normal_switch:
    ; we are now on the kernel stack
    ; restore callee-saved registers
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbx
    popf
    pop     rbp
    ret
.end:

global x86_jump_to_userspace:function (x86_jump_to_userspace.end - x86_jump_to_userspace)
x86_jump_to_userspace:
    mov     rsp, rdi

    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     r11
    pop     r10
    pop     r9
    pop     r8

    pop     rdi
    pop     rsi
    pop     rbp

    pop     rdx
    pop     rcx
    pop     rbx
    pop     rax

    add     rsp, 2 * REGSIZE
    iretq
.end:
