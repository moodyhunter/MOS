; SPDX-License-Identifier: GPL-3.0-or-later
section .text

global gdt32_flush:function (reload_cs.end - gdt32_flush)
global idt32_flush:function (idt32_flush.end - idt32_flush)
global tss32_flush:function (tss32_flush.end - tss32_flush)
global gdt32_flush_only:function (gdt32_flush_only.end - gdt32_flush_only)

gdt32_flush:
   lgdt [rdi]
   push 0x08                 ; Push code segment to stack, 0x08 is a stand-in for your code segment
   lea  rax, reload_cs
   push rax                  ; Push this value to the stack
   retfq                     ; Perform a far return, RETFQ or LRETQ depending on syntax
reload_cs:
   ; Reload data segment registers
   mov  ax, 0x10 ; 0x10 is a stand-in for your data segment
   mov  ds, ax
   mov  es, ax
   mov  fs, ax
   mov  gs, ax
   mov  ss, ax
   ret
.end:

idt32_flush:
    mov rax, [rsp + 4]
    lidt [rax]
    ret
.end:

tss32_flush:
    mov rax, [rsp + 4]
    ltr ax
    ret
.end:

gdt32_flush_only:
    mov rax, [rsp + 4]
    lgdt [rax]
    ret
.end:
