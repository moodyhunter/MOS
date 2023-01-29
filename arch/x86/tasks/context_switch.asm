; SPDX-License-Identifier: GPL-3.0-or-later

%define ARG1 [ebp + 8]
%define ARG2 [ebp + 12]
%define ARG3 [ebp + 16]
%define ARG4 [ebp + 20]
%define ARG5 [ebp + 24]

; void x86_context_switch_impl(uintptr_t *old_stack, uintptr_t kernel_stack, uintptr_t pgd, uintptr_t jump_addr, uintptr_t x86_context)
global x86_context_switch_impl:function (x86_context_switch_impl.end - x86_context_switch_impl)
x86_context_switch_impl:
    push    ebp
    mov     ebp, esp

    ; save callee-saved registers
    push    ebx
    push    edi
    push    esi

    mov     eax, ARG1       ; eax = *old_stack
    mov     [eax], esp      ; save esp
    mov     eax, ARG2       ; eax = kernel_stack
    mov     ebx, ARG3       ; ebx = pgd
    mov     ecx, ARG4       ; ecx = jump_addr
    mov     edx, ARG5       ; edx = x86_context
    mov     esp, eax        ; set esp

    ; set up page directory if needed
    cmp     ebx, 0          ; if pgd == 0
    je      .skip_pgd_setup ;     don't update cr3
    mov     cr3, ebx        ; load page directory

.skip_pgd_setup:
    xor     eax, eax
    xor     ebx, ebx
    ; ecx contains jump_addr
    ; edx contains arguments for the new thread, don't clear it
    ; edx = struct { eip, stack, x86_stack_frame, arg; };
    xor     esi, esi
    xor     edi, edi
    xor     ebp, ebp
    ; jump to jump_addr
    jmp     ecx
.end:


global x86_switch_impl_normal:function (x86_switch_impl_normal.end - x86_switch_impl_normal)
x86_switch_impl_normal:
    ; we are now on the kernel stack
    ; restore callee-saved registers
    pop     esi
    pop     edi
    pop     ebx
    pop     ebp
    ret
.end:

global x86_switch_impl_new_kernel_thread:function (x86_switch_impl_new_kernel_thread.end - x86_switch_impl_new_kernel_thread)
x86_switch_impl_new_kernel_thread:
    ; we are now on the kernel stack, it's empty thus nothing to restore
    mov     eax, [edx]          ; eax = eip
    push    dword [edx + 21 * 4]; push the argument
    push    0                   ; push a dummy return address
    xor     edx, edx            ; clear edx
    jmp     eax                 ; jump to eip
.end:

[extern x86_switch_impl_setup_user_thread:function]

global x86_switch_impl_new_user_thread:function (x86_switch_impl_new_user_thread.end - x86_switch_impl_new_user_thread)
x86_switch_impl_new_user_thread:
    ; we are now on the kernel stack of the corresponding thread
    ; edx = struct { eip, stack, x86_stack_frame, arg; }; (size: 1, 1, 19, 1)

    ; ebx, ebp, edi, esi are callee-saved registers
    mov     edi, [edx + 2 * 4 + 4 * 4]
    mov     esi, [edx + 2 * 4 + 5 * 4]
    mov     ebp, [edx + 2 * 4 + 6 * 4]
    mov     ebx, [edx + 2 * 4 + 8 * 4]
    push    dword [edx]     ; save eip

    call    x86_switch_impl_setup_user_thread
    ; the function returns the new stack pointer in eax

    pop     ecx             ; pop eip

    ; set up the iret frame
    push    0x20 | 0x3      ; user data (stack) segment + RPL 3
    push    eax             ; stack
    push    0x200           ; push flags (IF = 1)
    push    0x18 | 0x3      ; user code segment + RPL 3
    push    ecx             ; eip

    ;! note: for process initialization, eax is 0
    ;!       for a child process after fork, eax is also 0
    ;!       so here we have to set eax to 0
    xor     eax, eax        ; clear eax
    xor     ecx, ecx        ; clear ecx
    xor     edx, edx        ; clear edx

    mov     cx, 0x20 | 3    ; user data segment + RPL 3
    mov     ds, cx
    mov     es, cx
    mov     fs, cx
    mov     gs, cx
    iret
.end:
