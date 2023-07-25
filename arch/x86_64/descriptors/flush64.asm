; SPDX-License-Identifier: GPL-3.0-or-later
section .text

global gdt_flush:function (gdt_flush.end - gdt_flush)
global idt_flush:function (idt_flush.end - idt_flush)
global tss_flush:function (tss_flush.end - tss_flush)
global gdt_flush_only:function (gdt_flush_only.end - gdt_flush_only)

gdt_flush:
    lgdt [rdi]

    mov ax, 0x20
    mov ss, ax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push qword 0x10
    lea rax, [rel .ret]
    push rax
    retfq
.ret:
    ret
.end:

idt_flush:
    lidt [rdi]
    ret
.end:

tss_flush:
    ltr di
    ret
.end:

gdt_flush_only:
    lgdt [rdi]
    ret
.end:

[section .data]
_gdtr:
    dw 0
    dd 0
