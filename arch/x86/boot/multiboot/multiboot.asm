[bits 32]

constants:
    STACK_ADDR  equ 0x00f00000      ; Stack starts at the 16MB address & grows down
    MB_MAGIC    equ 0x1BADB002      ; 'magic number' lets bootloader find the header
    FLAG_ALIGN  equ 1 << 0          ; 4KB alignment for the bootloader
    FLAG_MEM    equ 1 << 1          ; provide memory map
    FLAG_VIDEO  equ 0 << 2          ; provide video mode information

    VIDEO_MEM   equ 0xb8001         ; x86 text mode output

    MB_FLAGS    equ FLAG_ALIGN | FLAG_MEM | FLAG_VIDEO
    MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)                  ; checksum of above, to prove we are multiboot

section .multiboot.data
    align   4
    dd      MB_MAGIC
    dd      MB_FLAGS
    dd      MB_CHECKSUM

section .multiboot.text

extern x86_startup_setup
extern x86_start_kernel
extern __MOS_STARTUP_STACK_TOP

extern kernel_higher_stack
extern kernel_higher_initrd_addr
extern kernel_higher_initrd_size

global mos_x86_multiboot_start:function (mos_x86_multiboot_start.end - mos_x86_multiboot_start)

mos_x86_multiboot_start:
    mov     esp, __MOS_STARTUP_STACK_TOP
    push    0
    mov     ebp, esp
    push    0                           ; Reset EFLAGS
    popf
    push    ebx                         ; Push multiboot2 header pointer
    push    eax                         ; Push multiboot2 magic value[extern x86_start_kernel]
    call    x86_startup_setup           ; start the kernel

    ; ! TODO: Switch to higher half stack
    ; ! TOOD: Jump to higher half
    ; ! TODO: Give higher half of the kernel its initrd

    call    x86_start_kernel

.hang:
    hlt
    jmp .hang
.end:
