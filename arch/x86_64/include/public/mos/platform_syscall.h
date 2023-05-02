// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/types.h>

should_inline reg_t platform_syscall0(reg_t number)
{
    reg_t result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number));
    return result;
}

should_inline reg_t platform_syscall1(reg_t number, reg_t arg1)
{
    reg_t result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1));
    return result;
}

should_inline reg_t platform_syscall2(reg_t number, reg_t arg1, reg_t arg2)
{
    reg_t result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2));
    return result;
}

should_inline reg_t platform_syscall3(reg_t number, reg_t arg1, reg_t arg2, reg_t arg3)
{
    reg_t result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3));
    return result;
}

should_inline reg_t platform_syscall4(reg_t number, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4)
{
    reg_t result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4));
    return result;
}

should_inline reg_t platform_syscall5(reg_t number, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4, reg_t arg5)
{
    reg_t result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5));
    return result;
}

should_inline reg_t platform_syscall6(reg_t number, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4, reg_t arg5, reg_t arg6)
{
    reg_t result = 0;
    __asm__ volatile("push %%rbx\n" // callee-saved
                     "push %%rdi\n" // callee-saved
                     "push %%rsi\n" // callee-saved
                     "push %%rbp\n" // callee-saved

                     "mov %1, %%rax\n"
                     "mov %2, %%rbx\n"
                     "mov %3, %%rcx\n"
                     "mov %4, %%rdx\n"
                     "mov %5, %%rsi\n"
                     "mov %6, %%rdi\n"
                     "mov %7, %%rbp\n"

                     "int $0x88\n"

                     "pop %%rbp\n"
                     "pop %%rsi\n"
                     "pop %%rdi\n"
                     "pop %%rbx\n"
                     : "=a"(result)
                     : "m"(number), "m"(arg1), "m"(arg2), "m"(arg3), "m"(arg4), "m"(arg5), "m"(arg6));
    return result;
}
