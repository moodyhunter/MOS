; SPDX-License-Identifier: GPL-3.0-or-later

[bits 16]
%include "../gdt.asm"
switch_to_pm:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0            ; To make the switch to protected mode , we set
    or  eax, 0x1            ; the first bit of CR0 , a control register
    mov cr0, eax            ; Update the control register
    jmp GDT_CODE_SEG:pm_init


[bits 32]
pm_init:
    mov ax, GDT_DATA_SEG
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000         ; Set the stack pointer to 0x9000
    mov esp, ebp            ; Set the stack pointer to 0x9000
    call pm_start

