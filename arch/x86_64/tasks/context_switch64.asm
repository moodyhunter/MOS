; SPDX-License-Identifier: GPL-3.0-or-later

%define REGSIZE 8

; void x86_context_switch_impl(ptr_t *old_stack, ptr_t kernel_stack, ptr_t pgd, ptr_t jump_addr, ptr_t x86_context)
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

    ; rdi = *old_stack
    ; rsi = kernel_stack
    ; rdx = pgd
    ; rcx = jump_addr
    ; r8 = x86_context
    ; set rsp to kernel_stack
    mov     [rdi], rsp      ; backup old stack pointer
    mov     rsp, rsi        ; switch to kernel stack

    ; set up page directory if needed
    cmp     rdx, 0          ; if pgd == 0
    je      .skip_pgd_setup ;     don't update cr3
    mov     cr3, rdx        ; load page directory

.skip_pgd_setup:
    xor     rax, rax        ; clear rax, rbx, rsi, rdi, rbp
    xor     rbx, rbx
    xor     rdx, rdx        ; don't need rdx anymore
    xor     rsi, rsi
    mov     rdi, r8         ; rdi = x86_context
    xor     r8, r8
    xor     rbp, rbp
    ; rcx contains jump_addr
    ; rdi contains arguments for the new thread, don't clear it
    ; rdi = struct { x86_stack_frame, arg };
    ; jump to jump_addr
    jmp     rcx
.end:


global x86_switch_impl_normal:function (x86_switch_impl_normal.end - x86_switch_impl_normal)
x86_switch_impl_normal:
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

global x86_switch_impl_new_kernel_thread:function (x86_switch_impl_new_kernel_thread.end - x86_switch_impl_new_kernel_thread)
x86_switch_impl_new_kernel_thread:
    ; we are now on the kernel stack, it's empty thus nothing to restore
    mov     rax, [rdi + 19 * REGSIZE]   ; rax = eip
    mov     rdi, [rdi + 24 * REGSIZE]   ; the argument
    push    0                   ; push a dummy return address
    jmp     rax                 ; jump to eip
.end:

[extern x86_switch_impl_setup_user_thread:function]

global x86_switch_impl_new_user_thread:function (x86_switch_impl_new_user_thread.end - x86_switch_impl_new_user_thread)
x86_switch_impl_new_user_thread:
    ; we are now on the kernel stack of the corrrsponding thread
    ; rdi = struct { x86_stack_frame, arg }; (size: 24, 1)
    push    rdi
    call    x86_switch_impl_setup_user_thread
    pop     rdi

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
