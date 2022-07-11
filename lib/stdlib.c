// SPDX-License-Identifier: GPL-3.0-or-later

#include "stdlib.h"

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}
