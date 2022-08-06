// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"

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
    typedef intmax_t largeint_t;
    largeint_t *ldest = (largeint_t *) dest;
    largeint_t *lsrc = (largeint_t *) src;

    while (n >= sizeof(largeint_t))
    {
        *ldest++ = *lsrc++;
        n -= sizeof(largeint_t);
    }

    char *cdest = (char *) ldest;
    char *csrc = (char *) lsrc;

    while (n > 0)
    {
        *cdest++ = *csrc++;
        n--;
    }

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

void strcpy(char *dest, const char *src)
{
    while (*src)
        *dest++ = *src++;
    *dest = 0;
}

void strncpy(char *dest, const char *src, size_t n)
{
    while (n > 0 && *src)
    {
        *dest++ = *src++;
        n--;
    }
    while (n > 0)
    {
        *dest++ = 0;
        n--;
    }
}
