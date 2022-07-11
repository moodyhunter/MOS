; SPDX-License-Identifier: GPL-3.0-or-later

; ah: error code
; dl: disk drive that dropped the error
on_disk_error:
    mov si, msg_disk_read_error
    call print_string
    push dx
    mov dh, ah
    call print_hex
    jmp $

on_invalid_read_count:
    mov si, msg_read_sector_error
    call print_string
    mov dx, 0x00
    mov al, dl
    call print_hex
    jmp $


; read `dh` sectors from drive `dl` into `[ES:BX]`
; dl: drive number: `0x00: floppy; 0x01: floopy 2; 0x80: hdd; 0x81: hdd2`
disk_read:
    FUNCTION_READ equ 0x02
    READ_SECTOR equ 0x02                ; 0x01 is our boot sector, 0x02 is the first 'available' sector
    READ_CYLINDER equ 0x00
    READ_HEAD equ 0x00

    pusha                           ; save registers
    push dx                         ; save dh, to be used later

    mov ah, FUNCTION_READ           ; ah = int 0x13 function. 0x02 = 'read'
    mov al, dh                      ; al = number of sectors to read (0x01...0x80)
    mov ch, READ_CYLINDER           ; ch = cylinder (0x0...0x3FF, upper 2 bits in 'cl')
    mov cl, READ_SECTOR             ; cl = sector (0x01...0x11)
    mov dh, READ_HEAD               ; dh = head number (0x0...0xF)
    ; dl: set by the caller

    int 0x13                        ; fire off the interrupt
    jc on_disk_error

    pop dx
    cmp al, dh                      ; 'al' has the number of sectors that actually get read, compare
    jne on_invalid_read_count
    popa

    mov si, msg_ok
    call print_string
    ret
