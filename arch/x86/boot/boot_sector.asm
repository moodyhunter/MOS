base:
    bits 16                 ; 16 bit code
    [org 0x7C00]            ; where the code will be located

start:
    mov si, welcome         ; welcome message
    call print_string

    call print_newline

    mov si, base_address    ; base address
    call print_string
    mov dx, base
    call print_hex

    call print_newline

    mov si, strings_address ; strings address
    call print_string
    mov dx, strings
    call print_hex

    call print_newline

    mov si, padding_address ; padding address
    call print_string
    mov dx, padding
    call print_hex

    call print_newline

    jmp $                   ; go and jump till the end of the world

%include "utils.asm"

strings:
    welcome         db "=========================", 0x0A, 0x0D
                    db "|  Welcome to the MOS!  |", 0x0A, 0x0D
                    db "=========================", 0x0A, 0x0D, 0x00
    base_address    db "base:    ", 0x00
    strings_address db "strings: ", 0x00
    padding_address db "padding: ", 0x00

; padding up to the 512 - 2'th byte.
; we are filling the boot sector with zeros
padding:
    times 510 - ($ - $$) db 0


; boot sector magic value, to indicate that this 'is' a boot sector
.magic:
    ; dw 0xaa55 would also work (note the small endianness)
    db 0x55
    db 0xaa
