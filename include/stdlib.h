// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "types.h"

#define MOS_ASSERT(cond)                                                                                                                                                 \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (!(cond))                                                                                                                                                     \
            panic("Assertion failed: "##cond);                                                                                                                           \
    } while (0)

bool isspace(uchar c);
s32 atoi(const char *nptr);
