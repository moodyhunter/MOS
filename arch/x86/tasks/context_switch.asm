; SPDX-License-Identifier: GPL-3.0-or-later

%define arg_1 [esp + 4]
%define arg_2 [esp + 8]
%define arg_3 [esp + 12]

; void x86_context_switch_impl(uintptr_t *old_stack, uintptr_t new_stack, uintptr_t pgd)
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

    ; TODO do we save segment registers?
    mov     [eax], esp      ; save esp
.load_new:
    mov     esp, edx        ; load new esp
    cmp     ecx, 0          ; if pgd == 0
    je      .finalise_pop   ;     pop
    mov     cr3, ecx        ; load page directory
.finalise_pop:
    pop     esi
    pop     edi
    pop     ebx
	pop	    ebp
    ret
.end:

; void x86_um_thread_startup(void)
global x86_um_thread_startup:function (x86_um_thread_startup.end - x86_um_thread_startup)
x86_um_thread_startup:
    cli
    ; We are currently on the thread's stack
    ; LO [                arg, entry_point] HI
    pop     ecx                 ; arg
    pop     eax                 ; entry_point
    push    ecx                 ; arg
    push    0                   ; fake return address
    mov     ebx, esp            ; save stack pointer

    ; eax = entry_point
    ; ebx = stack_addr
    ; ecx = arg
    ; LO [           fake_return_addr, arg] HI

    push    0x20 | 0x3      ; user data (stack) segment + RPL 3
    push    ebx             ; stack
    pushf                   ; push flags
    push    0x18 | 0x3      ; user code segment + RPL 3
    push    eax             ; context->eip

    xor     eax, eax        ; clear eax
    xor     esi, esi        ; clear esi
    xor     edi, edi        ; clear edi
    xor     ebx, ebx        ; clear ebx

    mov     cx, 0x20 | 3    ; user data segment + ring 3
    mov     ds, cx
    mov     es, cx
    mov     fs, cx
    mov     gs, cx

    iret
.end:
