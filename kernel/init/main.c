// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mos_global.h"

int syscall0(int number)
{
    int result = 0;
    __asm__ volatile("int $0x88" : "=a"(result) : "a"(number));
    return result;
}

void main()
{
    int x = syscall0(1313);

    while (x < 40)
    {
        x += 1;
    }
}
