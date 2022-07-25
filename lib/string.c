// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/string.h"

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len])
        len++;
    return len;
}

s32 strcmp(const char *s1, const char *s2)
{
    size_t i = 0;
    while (s1[i] && s2[i] && s1[i] == s2[i])
        i++;
    return s1[i] - s2[i];
}

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
    u8 *d = dest;
    const u8 *s = src;
    for (size_t i = 0; i < n; i++)
        d[i] = s[i];
    return dest;
}

void *memmove(void *dest, const void *src, size_t n)
{
    u8 *d = dest;
    const u8 *s = src;
    if (d < s)
        for (size_t i = 0; i < n; i++)
            d[i] = s[i];
    else
        for (size_t i = n; i > 0; i--)
            d[i - 1] = s[i - 1];
    return dest;
}

void *memset(void *s, int c, size_t n)
{
    u8 *d = s;
    for (size_t i = 0; i < n; i++)
        d[i] = c;
    return s;
}
