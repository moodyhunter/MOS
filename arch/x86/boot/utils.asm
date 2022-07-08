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
