; SPDX-License-Identifier: GPL-3.0-or-later
section .text

global gdt32_flush:function (gdt32_flush.end - gdt32_flush)
global idt32_flush:function (idt32_flush.end - idt32_flush)

gdt32_flush:
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

idt32_flush:
    mov eax, [esp + 4]
    lidt [eax]
    ret
.end:
