// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mos_global.h"
#include "mos/types.h"

always_inline s32 syscall0(s32 number)
{
    s32 result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number));
    return result;
}

always_inline s32 syscall1(s32 number, s32 arg1)
{
    s32 result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1));
    return result;
}

always_inline s32 syscall2(s32 number, s32 arg1, s32 arg2)
{
    s32 result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2));
    return result;
}

always_inline s32 syscall3(s32 number, s32 arg1, s32 arg2, s32 arg3)
{
    s32 result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number), "b"(arg1), "c"(arg2), "d"(arg3));
    return result;
}
