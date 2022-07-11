; SPDX-License-Identifier: GPL-3.0-or-later

[bits 32]
[extern start_kernel]

section .kernel_entry_asm
global call_kernel_entry
call_kernel_entry:
    call start_kernel
    jmp $
