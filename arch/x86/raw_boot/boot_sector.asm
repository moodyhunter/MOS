; SPDX-License-Identifier: GPL-3.0-or-later

[bits 16]                   ; 16 bit code
[org 0x7C00]                ; where the code will be located

; ! Keep this the same as in the kernel linker script !
KERNEL_BINARY_OFFSET equ 0x1000 ; where the kernel binary is located


start:
    mov bp, 0x8000          ; set stack pointer to be away from the code
    mov sp, bp

    mov [BOOT_DEVICE], dl   ; save boot device in a global variable

    call print_newline
    mov si, welcome         ; welcome message
    call print_string
    call print_newline

    mov si, msg_loading_kernel
    call print_string

    mov dl, [BOOT_DEVICE]   ; get boot device
    mov dh, 0x02            ; read 2 sectors
    mov bx, KERNEL_BINARY_OFFSET
    call disk_read          ; read the kernel from the boot drive

    call print_newline
    mov si, msg_entering_pm
    call print_string

    call switch_to_pm
    jmp $                   ; we won't get here

%include "rm_utils.asm"
%include "rm_diskio.asm"

%include "pm_switch.asm"

[bits 32]
pm_start:
    mov ebx, msg_kstarting
    call pm_print_string
    call KERNEL_BINARY_OFFSET
    mov ebx, msg_kterminated
    call pm_print_string
    jmp $

%include "pm_utils.asm"

globals:
    BOOT_DEVICE             db 0x00

strings:
    welcome                 db "=====================", 0x0A, 0x0D
                            db "|  Welcome to MOS!  |", 0x0A, 0x0D
                            db "=====================", 0x0A, 0x0D, 0x00
    msg_loading_kernel      db "Loading kernel... ", 0x00
    msg_disk_read_error     db "error, ec:dev=", 0x00
    msg_read_sector_error   db "error, read sector#", 0x00
    msg_ok                  db "ok", 0x00
    msg_entering_pm         db "Entering protected mode...", 0x0A, 0x0D, 0x00
    msg_kstarting           db "STARTING!!!!!!.", 0x00
    msg_kterminated         db "TERMINATED.", 0x00



; padding up to the 512 - 2'th byte.
; we are filling the boot sector with zeros
padding:
    times 510 - ($ - $$) db 0


; boot sector magic value, to indicate that this 'is' a boot sector
.magic:
    ; dw 0xaa55 would also work (note the small endianness)
    db 0x55
    db 0xaa
