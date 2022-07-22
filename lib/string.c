// SPDX-License-Identifier: GPL-3.0-or-later

#include "string.h"

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

s8 strcmp(const char *s1, const char *s2)
{
    size_t i = 0;
    while (s1[i] && s2[i] && s1[i] == s2[i])
        i++;
    return s1[i] - s2[i];
}
