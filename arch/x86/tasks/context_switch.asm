; SPDX-License-Identifier: GPL-3.0-or-later

%define arg_1 [esp + 4]
%define arg_2 [esp + 8]
%define arg_3 [esp + 12]

; void x86_context_switch_impl(uintptr_t *old_stack, uintptr_t new_stack, uintptr_t pgd, uintptr_t post_switch_op, uintptr_t post_switch_op_arg)
global x86_context_switch_impl:function (x86_context_switch_impl.end - x86_context_switch_impl)
x86_context_switch_impl:
    mov     eax, arg_1  ; eax = old
    mov     edx, arg_2  ; edx = new
    mov     ecx, arg_3  ; ecx = page_dir
    push    ebp
    mov     ebp, esp

    push    ebx
    push    edi
    push    esi

    mov     [eax], esp      ; save esp
    mov     eax, [esp + 32] ; eax = post_switch_op
    mov     ebx, [esp + 36] ; ebx = post_switch_op_arg
    mov     esp, edx        ; load new esp
    cmp     ecx, 0          ; if pgd == 0
    je      .skip_pgd       ;     pop
    mov     cr3, ecx        ; load page directory
.skip_pgd:
    cmp     eax, 0          ; if post_switch_op == 0
    jne     .has_post_switch_op
    pop     esi
    pop     edi
    pop     ebx
	pop	    ebp
    ret
.has_post_switch_op:
    push    ebx             ; push post_switch_op_arg
    call    eax             ; call post_switch_op
.end:


; void x86_um_thread_post_fork(void)
global x86_um_thread_post_fork:function (x86_um_thread_post_fork.end - x86_um_thread_post_fork)
x86_um_thread_post_fork:
    ; We are currently on the thread's stack, and using its own page directory
    ; Writing to the stack causes a page fault.
    ; LO [                            post_switch_op_arg] HI
    pop     eax             ; pop post_switch_op_arg
    push    eax             ; push post_switch_op_arg
    push    eax             ; push post_switch_op_arg (again, we'll jump to x86_um_thread_startup which discards the first one)

    jmp     x86_um_thread_startup
.end:


; void x86_um_thread_startup(void)
global x86_um_thread_startup:function (x86_um_thread_startup.end - x86_um_thread_startup)
x86_um_thread_startup:
    ; We are currently on the thread's stack
    ; LO [          post_switch_op_arg, entry_point, arg] HI
    add     esp, 8          ; pop post_switch_op_arg (we don't need it)
    pop     eax             ; entry_point
    push    0               ; fake return address
    mov     ebx, esp        ; save stack pointer

    ; eax = entry_point
    ; ebx = stack pointer
    ; LO [                         fake_return_addr, arg] HI

    push    0x20 | 0x3      ; user data (stack) segment + RPL 3
    push    ebx             ; stack
    pushf                   ; push flags

    ; enable interrupts
    pop     edx             ; pop flags
    or      edx, 0x200      ; set IF
    push    edx             ; push flags

    push    0x18 | 0x3      ; user code segment + RPL 3
    push    eax             ; entry_point

    xor     eax, eax        ; clear eax
    xor     esi, esi        ; clear esi
    xor     edi, edi        ; clear edi
    xor     ebx, ebx        ; clear ebx

    mov     cx, 0x20 | 3    ; user data segment + RPL 3
    mov     ds, cx
    mov     es, cx
    mov     fs, cx
    mov     gs, cx

    iret
.end:

