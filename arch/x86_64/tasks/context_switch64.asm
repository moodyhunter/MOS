; SPDX-License-Identifier: GPL-3.0-or-later

%define REGSIZE 8

; void x86_context_switch_impl(ptr_t x86_context, ptr_t *old_stack, ptr_t kernel_stack, ptr_t pgd, ptr_t jump_addr)
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
    ; rcx = pgd
    ; r8 = jump_addr
    ; set rsp to kernel_stack
    mov     [rsi], rsp      ; backup old stack pointer
    mov     rsp, rdx        ; switch to kernel stack

    ; set up page directory if needed
    cmp     rcx, 0          ; if pgd == 0
    je      .skip_pgd_setup ;     don't update cr3
    mov     cr3, rcx        ; load page directory

.skip_pgd_setup:
    xor     rax, rax        ; clear rax, rbx, rsi, rdi, rbp
    xor     rbx, rbx
    xor     rcx, rcx
    xor     rdx, rdx
    xor     rsi, rsi
    xor     rbp, rbp
    ; r8 contains jump_addr
    ; rdi contains arguments for the new thread, don't clear it
    ; rdi = struct { x86_stack_frame, arg };
    ; jump to jump_addr
    jmp     r8
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
    ; set up the iret frame
    push    0x40 | 0x3                  ; user data (stack) segment + RPL 3
    push    qword [rdi + 22 * REGSIZE]  ; stack
    push    qword [rdi + 21 * REGSIZE]  ; rflags (IF = 1)
    push    0x30 | 0x3                  ; user code segment + RPL 3
    push    qword [rdi + 19 * REGSIZE]  ; ip

    ; restore callee-saved registers (RBX, RSP, RBP, and R12-R15)
    mov     r15, [rdi + 2 * REGSIZE]
    mov     r14, [rdi + 3 * REGSIZE]
    mov     r13, [rdi + 4 * REGSIZE]
    mov     r12, [rdi + 5 * REGSIZE]
    mov     rsi, [rdi + 11 * REGSIZE]
    mov     rbp, [rdi + 12 * REGSIZE]
    mov     rbx, [rdi + 15 * REGSIZE]
    mov     rdi, [rdi + 10 * REGSIZE]

    mov     cx, 0x40 | 3    ; user data segment + RPL 3
    mov     ds, cx
    mov     es, cx
    mov     fs, cx
    mov     gs, cx

    ;! note: for process initialization, rax is 0
    ;!       for a child process after fork, rax is also 0
    ;!       so here we have to set rax to 0
    xor     rax, rax        ; clear rax
    xor     rcx, rcx        ; clear rcx
    xor     rdx, rdx        ; clear rdx

    iretq
.end:
