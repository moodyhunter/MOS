[bits 32]


%define MB_MAGIC    0x1BADB002      ; 'magic number' lets bootloader find the header
%define FLAG_ALIGN  1 << 0          ; 4KB alignment for the bootloader
%define FLAG_MEM    1 << 1          ; provide memory map
%define FLAG_VIDEO  1 << 2          ; provide video mode information

MB_FLAGS    equ FLAG_ALIGN | FLAG_MEM
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

section .multiboot.data
    dd      MB_MAGIC
    dd      MB_FLAGS
    dd      MB_CHECKSUM
section .multiboot.text

extern x86_startup
extern x86_multiboot_entry
extern _mos_startup_stack
extern __MOS_KERNEL_HIGHER_STACK_TOP

global mos_x86_multiboot_start:function (mos_x86_multiboot_start.end - mos_x86_multiboot_start)
mos_x86_multiboot_start:
    mov     esp, _mos_startup_stack
    push    0                           ; Reset EFLAGS
    popf

    ; on-stack struct x86_startup_info
    push    ebx                         ; Push multiboot2 header pointer
    push    eax                         ; Push multiboot2 magic value

    push    esp                         ; Push [struct x86_startup_info] pointer
    call    x86_startup                 ; prepare for a higher half kernel
    pop     eax                         ; garbage ???
    mov     eax, esp                    ; save the pointer to the struct x86_startup_info

    mov     esp, __MOS_KERNEL_HIGHER_STACK_TOP ; paging has been enabled, switch to higher stack
    push    dword [eax + 4]             ; Push multiboot2 header pointer
    push    dword [eax]                 ; Push multiboot2 magic value
    push    esp                         ; Push [struct x86_startup_info] pointer
    call    x86_multiboot_entry
.hang:
    hlt
    jmp     .hang
.end:
