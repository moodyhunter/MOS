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

    ; rdi = x86_stack_frame
    ; rsi = *old_stack
    ; rdx = kernel_stack
    ; rcx = jump_addr
    ; set rsp to kernel_stack
    mov     [rsi], rsp      ; backup old stack pointer
    mov     rsp, rdx        ; switch to kernel stack

    xor     rax, rax        ; clear rax, rbx, rdx, rsi, rbp
    xor     rbx, rbx
    xor     rdx, rdx
    xor     rsi, rsi
    xor     rbp, rbp
    ; rdi = struct x86_stack_frame;
    jmp     rcx             ; rcx contains jump_addr
.end:

global x86_normal_switch_impl:function (x86_normal_switch_impl.end - x86_normal_switch_impl)
x86_normal_switch_impl:
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
