; SPDX-License-Identifier: GPL-3.0-or-later

[bits 32]

call_kernel_entry:
    section .kernel_entry_asm           ; used by the linker script to place this code at the start of the kernel
    global call_kernel_entry            ; export the entry point for the kernel
    [extern start_kernel]               ; the REAL kernel entry point
    call start_kernel
    jmp $
