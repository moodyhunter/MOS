    bits 16                 ; 16 bit code
    [org 0x7C00]              ; where the code will be located

start:
    mov si, welcome
    call print_string
    jmp $                   ; go and jump till the end of the world

%include "utils.asm"

strings:
    welcome db "Welcome to the MOS!", 0x00

; padding up to the 512 - 2'th byte.
; we are filling the boot sector with zeros
padding:
    times 510 - ($ - $$) db 0


; boot sector magic value, to indicate that this 'is' a boot sector
.magic:
    ; dw 0xaa55 would also work (note the small endianness)
    db 0x55
    db 0xaa
