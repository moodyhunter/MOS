; SPDX-License-Identifier: GPL-3.0-or-later
section .text

global gdt_flush:function (gdt_flush.end - gdt_flush)
global idt_flush:function (idt_flush.end - idt_flush)
global tss_flush:function (tss_flush.end - tss_flush)
global gdt_flush_only:function (gdt_flush_only.end - gdt_flush_only)

gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]
    ; 0x10 is the offset in the GDT to our data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov	gs, ax
    mov ss, ax
    jmp	0x08:.flush  ; Long jump to our new code segment
.flush:
    ret
.end:

idt_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret
.end:

tss_flush:
    mov eax, [esp + 4]
    ltr ax
    ret
.end:

gdt_flush_only:
    mov eax, [esp + 4]
    lgdt [eax]
    ret
.end:
