// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "stdtypes.h"

#define MOS_ASSERT(cond)                                                                                                                                                 \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (!(cond))                                                                                                                                                     \
            panic("Assertion failed: "##cond);                                                                                                                           \
    } while (0)

u8 inb(u16 port);
u8 outb(u16 port, u8 value);

size_t strlen(const char *str);
