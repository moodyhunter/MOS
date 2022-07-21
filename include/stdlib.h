// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "stdtypes.h"

#define MOS_ASSERT(cond)                                                                                                                                                 \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (!(cond))                                                                                                                                                     \
            panic("Assertion failed: "##cond);                                                                                                                           \
    } while (0)

size_t strlen(const char *str);
s8 strcmp(const char *str1, const char *str2);
