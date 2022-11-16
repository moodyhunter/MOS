; SPDX-License-Identifier: GPL-3.0-or-later

; switch_mode:
;   0: Normal context switching by swapping esp
;   1: Interrupt return

; void x86_context_switch_impl(uintptr_t *old_stack, uintptr_t new_stack, uintptr_t pgd, int switch_mode, uintptr_t switch_mode_arg)
global x86_context_switch_impl:function (x86_context_switch_impl.end - x86_context_switch_impl)
x86_context_switch_impl:
    push    ebp
    mov     ebp, esp

    push    ebx
    push    edi
    push    esi

    mov     eax, [ebp + 8] ; eax = *old_stack
    mov     [eax], esp      ; save esp

    mov     eax, [ebp + 12] ; eax = new stack
    mov     ebx, [ebp + 16] ; ebx = pgd
    mov     ecx, [ebp + 20] ; ecx = switch_mode
    mov     edx, [ebp + 24] ; edx = switch_mode_arg

    cmp     ebx, 0          ; if pgd == 0
    je      .skip_pgd_setup ;     pop
    mov     cr3, ebx        ; load page directory

.skip_pgd_setup:
    cmp     ecx, 1          ; if switch_mode != 0
    je      .iret_switch    ;     iret
    mov     esp, eax        ; set esp

    pop     esi
    pop     edi
    pop     ebx
	pop	    ebp
    ret

.iret_switch:
    ; We are now on the kernel stack.
    ; edx = switch_mode_arg = struct { eip, ebp; };
    push    0x20 | 0x3      ; user data (stack) segment + RPL 3
    push    eax             ; stack
    mov     ebp, [edx + 4]  ; ebp
    pushf                   ; push flags

    ; enable interrupts
    pop     ebx             ; pop flags
    or      ebx, 0x200      ; set IF
    push    ebx             ; push flags

    push    0x18 | 0x3      ; user code segment + RPL 3
    push    dword [edx]     ; eip

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
