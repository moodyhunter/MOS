// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/syscall.h"
#include "mos/types.h"

void main(uintptr_t arg)
{
    s32 x = syscall0(arg);

    while (x < 40)
    {
        x += 1;
    }

    while (true)
        ;
}
