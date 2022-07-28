
.section .text
.global x86_gdt_flush

.type x86_gdt_flush, @function
    # void x86_gdt_flush(u32 gdt_ptr)
x86_gdt_flush:
    movl    4(%esp), %eax
    lgdt	(%eax)

    # 0x10 is the offset in the GDT to our data segment
    movw	$0x10, %ax
    movw	%ax, %ds
    movw	%ax, %es
    movw	%ax, %fs
    movw	%ax, %gs
    movw	%ax, %ss
    ljmp	$0x08, $.flush  # Long jump to our new code segment
.flush:
    ret


.size x86_gdt_flush, . - x86_gdt_flush
