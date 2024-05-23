.section .text_init
.global riscv64_simple_start

riscv64_simple_start:
	la sp, __MOS_KERNEL_HIGHER_STACK_TOP
	mv fp, sp
	j riscv_simple_main
bad:
	j bad

