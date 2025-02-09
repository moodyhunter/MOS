// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mos/mos_global.h>
#include <mos/types.hpp>

enum
{
    X86_SYSCALL_IOPL_ENABLE = 0,  // enable IO operations for the current process
    X86_SYSCALL_IOPL_DISABLE = 1, // disable IO operations for the current process
    X86_SYSCALL_SET_FS_BASE = 2,  // set the FS base address
    X86_SYSCALL_SET_GS_BASE = 3,  // set the GS base address
};

should_inline reg_t platform_syscall0(reg_t number)
{
    reg_t result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number) : "memory");
    return result;
}

should_inline reg_t platform_syscall1(reg_t number, reg_t arg1)
{
    reg_t result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1) : "memory");
    return result;
}

should_inline reg_t platform_syscall2(reg_t number, reg_t arg1, reg_t arg2)
{
    reg_t result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2) : "memory");
    return result;
}

should_inline reg_t platform_syscall3(reg_t number, reg_t arg1, reg_t arg2, reg_t arg3)
{
    reg_t result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3) : "memory");
    return result;
}

should_inline reg_t platform_syscall4(reg_t number, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4)
{
    reg_t result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4) : "memory");
    return result;
}

should_inline reg_t platform_syscall5(reg_t number, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4, reg_t arg5)
{
    reg_t result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5) : "memory");
    return result;
}

should_inline reg_t platform_syscall6(reg_t number, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4, reg_t arg5, reg_t arg6)
{
    reg_t result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4), "D"(arg5), "r"(arg6) : "memory");
    return result;
}
