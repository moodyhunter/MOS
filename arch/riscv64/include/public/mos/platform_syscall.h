// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/types.h>

should_inline reg_t platform_syscall0(reg_t number)
{
    reg_t ret;
    __asm__ volatile("ecall" : "=r"(ret) : "r"(number) : "memory");
    return ret;
}

should_inline reg_t platform_syscall1(reg_t number, reg_t arg0)
{
    reg_t ret;
    __asm__ volatile("ecall" : "=r"(ret) : "r"(number), "r"(arg0) : "memory");
    return ret;
}

should_inline reg_t platform_syscall2(reg_t number, reg_t arg0, reg_t arg1)
{
    reg_t ret;
    __asm__ volatile("ecall" : "=r"(ret) : "r"(number), "r"(arg0), "r"(arg1) : "memory");
    return ret;
}

should_inline reg_t platform_syscall3(reg_t number, reg_t arg0, reg_t arg1, reg_t arg2)
{
    reg_t ret;
    __asm__ volatile("ecall" : "=r"(ret) : "r"(number), "r"(arg0), "r"(arg1), "r"(arg2) : "memory");
    return ret;
}

should_inline reg_t platform_syscall4(reg_t number, reg_t arg0, reg_t arg1, reg_t arg2, reg_t arg3)
{
    reg_t ret;
    __asm__ volatile("ecall" : "=r"(ret) : "r"(number), "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3) : "memory");
    return ret;
}

should_inline reg_t platform_syscall5(reg_t number, reg_t arg0, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4)
{
    reg_t ret;
    __asm__ volatile("ecall" : "=r"(ret) : "r"(number), "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4) : "memory");
    return ret;
}

should_inline reg_t platform_syscall6(reg_t number, reg_t arg0, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4, reg_t arg5)
{
    reg_t ret;
    __asm__ volatile("ecall" : "=r"(ret) : "r"(number), "r"(arg0), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4), "r"(arg5) : "memory");
    return ret;
}
