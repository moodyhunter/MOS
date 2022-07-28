; SPDX-License-Identifier: GPL-3.0-or-later
section .text

global x86_gdt_flush:function (x86_gdt_flush.end - x86_gdt_flush)

x86_gdt_flush:
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
