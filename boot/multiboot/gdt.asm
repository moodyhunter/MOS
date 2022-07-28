; SPDX-License-Identifier: BSD-3-Clause
; Adapted from https://github.com/cfenollosa/os-tutorial/blob/master/09-32bit-gdt/32bit-gdt.asm

GDT_CODE_SEG equ gdt.code - gdt
GDT_DATA_SEG equ gdt.data - gdt

; %define


gdt:
; null descriptor
.null:
    ; the GDT starts with a null 8-byte
    dw 0, 0, 0, 0, 0, 0, 0, 0

    ; GDT for code segment. base = 0x00000000, length = 0xfffff
    ; for flags, refer to os-dev.pdf document, page 36
.code:
    dw 0xffff       ; segment length, bits 0-15
    dw 0x0          ; segment base, bits 0-15
    db 0x0          ; segment base, bits 16-23
    db 10011010b    ; flags (8 bits)
    db 11001111b    ; flags (4 bits) + segment length, bits 16-19
    db 0x0          ; segment base, bits 24-31

    ; GDT for data segment. base and length identical to code segment
    ; some flags changed, again, refer to os-dev.pdf
.data:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0

.end:

; GDT descriptor
gdt_descriptor:
    dw gdt.end - gdt - 1  ; size (16 bit), always one less of its true size
    dd gdt                ; address (32 bit)
