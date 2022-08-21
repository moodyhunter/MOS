; SPDX-License-Identifier: GPL-3.0-or-later
section .text

global gdt32_flush:function (gdt32_flush.end - gdt32_flush)
global idt32_flush:function (idt32_flush.end - idt32_flush)
global tss32_flush:function (tss32_flush.end - tss32_flush)
global gdt32_flush_only:function (gdt32_flush_only.end - gdt32_flush_only)

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

tss32_flush:
    mov eax, [esp + 4]
    ltr ax
    ret
.end:

gdt32_flush_only:
    mov eax, [esp + 4]
    lgdt [eax]
    ret
.end:
