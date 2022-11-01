// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mos_global.h"
#include "mos/types.h"

// TODO: rewrite in ASM

always_inline long platform_syscall0(long number)
{
    long result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number));
    return result;
}

always_inline long platform_syscall1(long number, long arg1)
{
    long result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1));
    return result;
}

always_inline long platform_syscall2(long number, long arg1, long arg2)
{
    long result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2));
    return result;
}

always_inline long platform_syscall3(long number, long arg1, long arg2, long arg3)
{
    long result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3));
    return result;
}

always_inline long platform_syscall4(long number, long arg1, long arg2, long arg3, long arg4)
{
    long result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4));
    return result;
}
