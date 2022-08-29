// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ksyscall.h"
#include "mos/mos_global.h"
#include "mos/types.h"

always_inline u32 syscall0(u32 number)
{
    u32 result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number));
    return result;
}

always_inline u32 syscall1(u32 number, u32 arg1)
{
    u32 result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1));
    return result;
}

always_inline u32 syscall2(u32 number, u32 arg1, u32 arg2)
{
    u32 result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2));
    return result;
}

always_inline u32 syscall3(u32 number, u32 arg1, u32 arg2, u32 arg3)
{
    u32 result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3));
    return result;
}

always_inline u32 syscall4(u32 number, u32 arg1, u32 arg2, u32 arg3, u32 arg4)
{
    u32 result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4));
    return result;
}
