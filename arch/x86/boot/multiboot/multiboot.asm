[bits 32]

constants:
    MB_MAGIC    equ 0x1BADB002      ; 'magic number' lets bootloader find the header
    FLAG_ALIGN  equ 1 << 0          ; 4KB alignment for the bootloader
    FLAG_MEM    equ 1 << 1          ; provide memory map
    FLAG_VIDEO  equ 1 << 2          ; provide video mode information

    MB_FLAGS    equ FLAG_ALIGN | FLAG_MEM
    MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot.data
    align   4
    dd      MB_MAGIC
    dd      MB_FLAGS
    dd      MB_CHECKSUM
section .multiboot.text

extern x86_startup
extern x86_start_kernel
extern __MOS_STARTUP_STACK_TOP
extern __MOS_KERNEL_HIGHER_STACK_TOP

extern initrd_size

global mos_x86_multiboot_start:function (mos_x86_multiboot_start.end - mos_x86_multiboot_start)
mos_x86_multiboot_start:
    mov     esp, __MOS_STARTUP_STACK_TOP
    push    0                           ; Reset EFLAGS
    popf
    push    ebx                         ; Push multiboot2 header pointer
    push    eax                         ; Push multiboot2 magic value[extern x86_start_kernel]
    call    x86_startup                 ; start the kernel
    pop     eax
    pop     ebx
    mov     esp, __MOS_KERNEL_HIGHER_STACK_TOP
    push    ebx                        ; Push multiboot2 header pointer
    push    dword [initrd_size]
    call    x86_start_kernel
.hang:
    hlt
    jmp     .hang
.end:
