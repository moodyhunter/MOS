; print a single character from the register al, using the INT 10h call
; **WARN: it's the caller's responsibility to preserve the previous value of AX**
print_char:
    mov ah, 0x0e
    int 0x10                ; fire the interrupt
    ret

print_newline:
    mov ah, 0x0e
    mov al, 0x0a
    int 0x10                ; fire the interrupt
    mov al, 0x0d
    int 0x10                ; fire the interrupt
    ret

; print a null-terminated string from `SI`
; **WARN: it's the caller's responsibility to make sure the string is null-terminated**
print_string:
    push ax
    mov ah, 0x0e
.loop:
    lodsb
    cmp al, 0x00            ; if the string is finished...
    je .done                ; return
    int 0x10                ; print the character
    jmp .loop
.done:
    pop ax
    ret


; prints a hexadecimal digit, given the number in `AL`
print_single_hex_digit:
    push ax
    and al, 0x0f
    cmp al, 0x09            ; if the number is greater than 9...
    jg .print_hex_high      ; print the high digit
.print_hex_lo:
    add al, '0'             ; otherwise, print the low digit (0-9)
    jmp .do_print
.print_hex_high:
    sub al, 0x0a
    add al, 'A'             ; otherwise, print the high digit (A-F)
.do_print:
    call print_char
    pop ax
    ret


; prints a number in the format `0x` in hexadecimal form, given the number in `DX`
; **WARN: it's the caller's responsibility to make sure the number is in the correct range**
print_hex:
    push ax
    push cx
    mov al, '0'
    call print_char
    mov al, 'x'
    call print_char
    mov cl, 12
.loop:
    mov ax, dx
    shr ax, cl
    call print_single_hex_digit
    sub cl, 4
    cmp cl, 0
    jns .loop
.leave:
    pop cx
    pop ax
    ret
