[bits 32]
[extern start_kernel]
call_kernel_entry:
    call start_kernel
    jmp $
