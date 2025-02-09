// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>
#include <mos/types.h>

enum
{
    RISCV64_SYSCALL_SET_TP = 0,
};

#define do_syscall(...) __asm__ volatile("ecall" : "=r"(a0) : __VA_ARGS__ : "memory")

should_inline reg_t platform_syscall0(long n)
{
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0");
    do_syscall("r"(a7));
    return a0;
}

should_inline reg_t platform_syscall1(long n, long a)
{
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0") = a;
    do_syscall("r"(a0), "r"(a7));
    return a0;
}

should_inline reg_t platform_syscall2(long n, long a, long b)
{
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0") = a;
    register long a1 __asm__("a1") = b;
    do_syscall("r"(a0), "r"(a7), "r"(a1));
    return a0;
}

should_inline reg_t platform_syscall3(long n, long a, long b, long c)
{
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0") = a;
    register long a1 __asm__("a1") = b;
    register long a2 __asm__("a2") = c;
    do_syscall("r"(a0), "r"(a7), "r"(a1), "r"(a2));
    return a0;
}

should_inline reg_t platform_syscall4(long n, long a, long b, long c, long d)
{
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0") = a;
    register long a1 __asm__("a1") = b;
    register long a2 __asm__("a2") = c;
    register long a3 __asm__("a3") = d;
    do_syscall("r"(a0), "r"(a7), "r"(a1), "r"(a2), "r"(a3));
    return a0;
}

should_inline reg_t platform_syscall5(long n, long a, long b, long c, long d, long e)
{
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0") = a;
    register long a1 __asm__("a1") = b;
    register long a2 __asm__("a2") = c;
    register long a3 __asm__("a3") = d;
    register long a4 __asm__("a4") = e;
    do_syscall("r"(a0), "r"(a7), "r"(a1), "r"(a2), "r"(a3), "r"(a4));
    return a0;
}

should_inline reg_t platform_syscall6(long n, long a, long b, long c, long d, long e, long f)
{
    register long a7 __asm__("a7") = n;
    register long a0 __asm__("a0") = a;
    register long a1 __asm__("a1") = b;
    register long a2 __asm__("a2") = c;
    register long a3 __asm__("a3") = d;
    register long a4 __asm__("a4") = e;
    register long a5 __asm__("a5") = f;
    do_syscall("r"(a0), "r"(a7), "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5));
    return a0;
}

#undef do_syscall
