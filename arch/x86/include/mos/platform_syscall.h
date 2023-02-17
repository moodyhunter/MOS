// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

enum
{
    X86_SYSCALL_IOPL_ENABLE = 0,    // enable IO operations for the current process
    X86_SYSCALL_IOPL_DISABLE = 1,   // disable IO operations for the current process
    X86_SYSCALL_MAP_VGA_MEMORY = 2, // map VGA memory to the current process
};

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
    __asm__ volatile("push %%ebx\n" // callee-saved
                     "push %%edi\n" // callee-saved
                     "push %%esi\n" // callee-saved
                     "push %%ebp\n" // callee-saved

                     "mov %1, %%eax\n"
                     "mov %2, %%ebx\n"
                     "mov %3, %%ecx\n"
                     "mov %4, %%edx\n"
                     "mov %5, %%esi\n"
                     "mov %6, %%edi\n"
                     "mov %7, %%ebp\n"

                     "int $0x88\n"

                     "pop %%ebp\n"
                     "pop %%esi\n"
                     "pop %%edi\n"
                     "pop %%ebx\n"
                     : "=a"(result)
                     : "m"(number), "m"(arg1), "m"(arg2), "m"(arg3), "m"(arg4), "m"(arg5), "m"(arg6));
    return result;
}
