    bits 16                 ; 16 bit code
    org 0x7C00              ; where the code will be located

start:
    mov si, welcome
    call print_string
    jmp $                   ; go and jump till the end of the world

; print a single character from the register al, using the INT 10h call
print_char:
    mov ah, 0x0e
    int 0x10                ; fire the interrupt
    ret

; print a string from the memory location given by si
; this is a recursive function
print_string:
    lodsb
    cmp al, 0x00            ; if the string is finished...
    je .done                ; return
    call print_char
    call print_string
.done:
    ret

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
