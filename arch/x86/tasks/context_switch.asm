; SPDX-License-Identifier: GPL-3.0-or-later

global x86_usermode_trampoline:function (x86_usermode_trampoline.end - x86_usermode_trampoline)
global x86_context_switch_impl:function (x86_context_switch_impl.end - x86_context_switch_impl)


x86_usermode_trampoline:
    cli
    add     esp, 4          ; remove ebp from the stack
    pop     edx             ; stack pointer
    pop     ebx             ; entry point (eip)
    pop     eax             ; eax = arg
    sub     edx, 4          ; push
    mov     [edx], eax      ; move arg to the new stack
    sub     edx, 4          ; push
    mov     dword [edx], 0  ; fake return address

    mov     cx, 0x20 | 3    ; user data segment + ring 3
    mov     ds, cx
    mov     es, cx
    mov     fs, cx
    mov     gs, cx

    push    0x20 | 3        ; user data segment + ring 3
    push    edx             ; esp
    pushf                   ; eflags
    push    0x18 | 3        ; user code segment + ring 3
    push    ebx             ; eip
    iret
.end:

; ! Keep the same layout as x86_context_t !
; as_context_t: { esp, eip }
; reg_t ebp;
; reg_t esi;
; reg_t edi;
; reg_t ebx;
; reg_t ds, es, fs, gs;

; void x86_context_switch_impl(x86_context_t *old, x86_context_t *new)
x86_context_switch_impl:
    mov     eax, [esp + 4]  ; eax = old
    mov     edx, [esp + 8]  ; edx = new
    cmp     eax, 0          ; if (old == 0)
    je      .load
    push    ebp
    push    ebx
    push    edi
    push    esi

    ; ? do we save segment registers ?
    mov     [eax], esp      ; save esp
.load:
    mov     esp, [edx]      ; load esp

    pop     esi
    pop     edi
    pop     ebx
    pop     ebp
    ret
.end:
