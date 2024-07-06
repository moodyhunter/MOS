// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/types.h>

enum
{
    RISCV64_SYSCALL_SET_TP = 0,
};

#define do_syscall(n, _a0, _a1, _a2, _a3, _a4, _a5)                                                                                                                      \
    register long a0 __asm__("a0") = _a0;                                                                                                                                \
    register long a1 __asm__("a1") = _a1;                                                                                                                                \
    register long a2 __asm__("a2") = _a2;                                                                                                                                \
    register long a3 __asm__("a3") = _a3;                                                                                                                                \
    register long a4 __asm__("a4") = _a4;                                                                                                                                \
    register long a5 __asm__("a5") = _a5;                                                                                                                                \
    register long syscall_id __asm__("a7") = n;                                                                                                                          \
    __asm__ volatile("ecall" : "+r"(a0) : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5), "r"(syscall_id));                                                                 \
    return a0

should_inline reg_t platform_syscall0(reg_t number)
{
    do_syscall(number, 0, 0, 0, 0, 0, 0);
}

should_inline reg_t platform_syscall1(reg_t number, reg_t arg0)
{
    do_syscall(number, arg0, 0, 0, 0, 0, 0);
}

should_inline reg_t platform_syscall2(reg_t number, reg_t arg0, reg_t arg1)
{
    do_syscall(number, arg0, arg1, 0, 0, 0, 0);
}

should_inline reg_t platform_syscall3(reg_t number, reg_t arg0, reg_t arg1, reg_t arg2)
{
    do_syscall(number, arg0, arg1, arg2, 0, 0, 0);
}

should_inline reg_t platform_syscall4(reg_t number, reg_t arg0, reg_t arg1, reg_t arg2, reg_t arg3)
{
    do_syscall(number, arg0, arg1, arg2, arg3, 0, 0);
}

should_inline reg_t platform_syscall5(reg_t number, reg_t arg0, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4)
{
    do_syscall(number, arg0, arg1, arg2, arg3, arg4, 0);
}

should_inline reg_t platform_syscall6(reg_t number, reg_t arg0, reg_t arg1, reg_t arg2, reg_t arg3, reg_t arg4, reg_t arg5)
{
    do_syscall(number, arg0, arg1, arg2, arg3, arg4, arg5);
}

#undef do_syscall
