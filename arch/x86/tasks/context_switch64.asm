; SPDX-License-Identifier: GPL-3.0-or-later

%define ARG1 [rbp + 8]
%define ARG2 [rbp + 12]
%define ARG3 [rbp + 16]
%define ARG4 [rbp + 20]
%define ARG5 [rbp + 24]

; void x86_context_switch_impl(ptr_t *old_stack, ptr_t kernel_stack, ptr_t pgd, ptr_t jump_addr, ptr_t x86_context)
global x86_context_switch_impl:function (x86_context_switch_impl.end - x86_context_switch_impl)
x86_context_switch_impl:
    push    rbp
    mov     rbp, rsp

    ; save callee-saved registers
    push    rbx
    push    rdi
    push    rsi
    pushf

    mov     rax, ARG1       ; rax = *old_stack
    mov     [rax], rsp      ; save rsp
    mov     rax, ARG2       ; rax = kernel_stack
    mov     rbx, ARG3       ; rbx = pgd
    mov     rcx, ARG4       ; rcx = jump_addr
    mov     rdx, ARG5       ; rdx = x86_context
    mov     rsp, rax        ; set rsp

    ; set up page directory if needed
    cmp     rbx, 0          ; if pgd == 0
    je      .skip_pgd_setup ;     don't update cr3
    mov     cr3, rbx        ; load page directory

.skip_pgd_setup:
    xor     rax, rax
    xor     rbx, rbx
    ; rcx contains jump_addr
    ; rdx contains arguments for the new thread, don't clear it
    ; rdx = struct { eip, stack, x86_stack_frame, arg; };
    xor     rsi, rsi
    xor     rdi, rdi
    xor     rbp, rbp
    ; jump to jump_addr
    jmp     rcx
.end:


global x86_switch_impl_normal:function (x86_switch_impl_normal.end - x86_switch_impl_normal)
x86_switch_impl_normal:
    ; we are now on the kernel stack
    ; restore callee-saved registers
    popf
    pop     rsi
    pop     rdi
    pop     rbx
    pop     rbp
    ret
.end:

global x86_switch_impl_new_kernel_thread:function (x86_switch_impl_new_kernel_thread.end - x86_switch_impl_new_kernel_thread)
x86_switch_impl_new_kernel_thread:
    ; we are now on the kernel stack, it's empty thus nothing to restore
    mov     rax, [rdx]          ; rax = eip
    push    qword [rdx + 21 * 4]; push the argument
    push    0                   ; push a dummy return address
    xor     rdx, rdx            ; clear rdx
    jmp     rax                 ; jump to eip
.end:

[extern x86_switch_impl_setup_user_thread:function]

global x86_switch_impl_new_user_thread:function (x86_switch_impl_new_user_thread.end - x86_switch_impl_new_user_thread)
x86_switch_impl_new_user_thread:
    ; we are now on the kernel stack of the corrrsponding thread
    ; rdx = struct { eip, stack, x86_stack_frame, arg; }; (size: 1, 1, 19, 1)

    ; rbx, rbp, rdi, rsi are callee-saved registers
    mov     rdi, [rdx + 2 * 4 + 4 * 4]
    mov     rsi, [rdx + 2 * 4 + 5 * 4]
    mov     rbp, [rdx + 2 * 4 + 6 * 4]
    mov     rbx, [rdx + 2 * 4 + 8 * 4]
    push    qword [rdx + 2 * 4 + 16 * 4] ; push the eflags
    push    qword [rdx]     ; save eip

    ; the x86_stack_frame may change during this call
    call    x86_switch_impl_setup_user_thread
    ; the function returns the new stack pointer in rax

    pop     rcx             ; pop eip
    pop     rdx             ; pop eflags

    ; set up the iret frame
    push    0x20 | 0x3      ; user data (stack) segment + RPL 3
    push    rax             ; stack
    push    rdx             ; push flags (IF = 1)
    push    0x18 | 0x3      ; user code segment + RPL 3
    push    rcx             ; eip

    ;! note: for process initialization, rax is 0
    ;!       for a child process after fork, rax is also 0
    ;!       so here we have to set rax to 0
    xor     rax, rax        ; clear rax
    xor     rcx, rcx        ; clear rcx
    xor     rdx, rdx        ; clear rdx

    mov     cx, 0x20 | 3    ; user data segment + RPL 3
    mov     ds, cx
    mov     es, cx
    mov     fs, cx
    mov     gs, cx
    iret
.end:
