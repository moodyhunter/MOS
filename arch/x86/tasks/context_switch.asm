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
    ; edx = struct { eip, stack, ebp, arg; };
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
    push    dword [edx + 12]    ; push the argument
    push    0                   ; push a dummy return address
    xor     edx, edx            ; clear edx
    jmp     eax                 ; jump to eip
.end:

global x86_switch_impl_new_user_thread:function (x86_switch_impl_new_user_thread.end - x86_switch_impl_new_user_thread)
x86_switch_impl_new_user_thread:
    ; we are now on the kernel stack of the corresponding thread
    ; edx = struct { eip, stack, ebp, arg; };
    mov     ebx, [edx + 4]  ; ebx = stack
    mov     ebp, [edx + 8]  ; ebp
    mov     eax, [edx + 12] ; eax = arg
    mov     ecx, [edx]      ; ecx = eip

    ; push the argument and a dummy return address
    sub     ebx, 4
    mov     [ebx], eax
    sub     ebx, 4
    mov     dword [ebx - 8], 0

    push    0x20 | 0x3      ; user data (stack) segment + RPL 3
    push    ebx             ; stack
    pushf                   ; push flags

    ; enable interrupts
    pop     ebx             ; pop flags
    or      ebx, 0x200      ; set IF
    push    ebx             ; push flags

    push    0x18 | 0x3      ; user code segment + RPL 3
    push    ecx             ; eip

    ;! note: for process initialization, eax is 0
    ;!       for a child process after fork, eax is also 0
    ;!       so here we have to set eax to 0
    xor     eax, eax        ; clear eax
    xor     ebx, ebx        ; clear ebx
    xor     ecx, ecx        ; clear ecx
    xor     edx, edx        ; clear edx
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
