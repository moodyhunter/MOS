; SPDX-License-Identifier: GPL-3.0-or-later

VIDEOMEM equ 0xb8000
WHITE_ON_BLACK equ 0x0f


; print a string to the screen, at ebx
pm_print_string:
    pusha               ; save registers
    mov edx, VIDEOMEM   ; set video memory address

.loop:
    mov al, [ebx]       ; get character from video memory
    cmp al, 0           ; if character is 0, stop
    je .done            ; if so, jump to end of loop

    mov ah, WHITE_ON_BLACK  ; set foreground color to white
    mov [edx], ax       ; write character to video memory

    inc ebx             ; increment pointer to next character
    add edx, 2          ; increment video memory address by 2
    jmp .loop           ; jump to top of loop

.done:
    popa
    ret
