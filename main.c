void _start()
{
    __asm("movl $0x0, %esp");
    __asm("movl $0x1, %eax");
    __asm("int $0x80");
}
