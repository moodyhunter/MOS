// SPDX-License-Identifier: GPL-3.0-or-later

#include "liballoc.h"

#include <mos/moslib_global.h>
#include <string.h>

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

s32 strncmp(const char *str1, const char *str2, size_t n)
{
    u8 c1, c2;
    while (n-- > 0)
    {
        c1 = (u8) *str1++;
        c2 = (u8) *str2++;
        if (c1 != c2)
            return c1 - c2;
        if (c1 == '\0')
            return 0;
    }
    return 0;
}

void *memcpy(void *restrict dest, const void *restrict src, size_t n)
{
    typedef s64 largeint_t;
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

void *memmove(void *dest, const void *source, size_t length)
{
    // https://github.com/eblot/newlib/blob/master/newlib/libc/string/memmove.c
    char *dst = dest;
    const char *src = source;

    if (src < dst && dst < src + length)
    {
        /* Have to copy backwards */
        src += length;
        dst += length;
        while (length--)
            *--dst = *--src;
    }
    else
    {
        while (length--)
            *dst++ = *src++;
    }

    return dest;
}

void *memset(void *s, int c, size_t n)
{
    u8 *d = s;
    for (size_t i = 0; i < n; i++)
        d[i] = c;
    return s;
}

void memzero(void *s, size_t n)
{
    typedef u64 largeint_t;
    largeint_t *lsrc = (largeint_t *) s;

    while (n >= sizeof(largeint_t))
    {
        *lsrc++ = 0;
        n -= sizeof(largeint_t);
    }

    char *csrc = (char *) lsrc;

    while (n > 0)
    {
        *csrc++ = 0;
        n--;
    }
}

char *strcpy(char *restrict dest, const char *restrict src)
{
    while (*src)
        *dest++ = *src++;
    *dest = 0;
    return dest;
}

char *strcat(char *restrict dest, const char *restrict src)
{
    while (*dest)
        dest++;
    while (*src)
        *dest++ = *src++;
    *dest = 0;
    return dest;
}

char *strncpy(char *restrict dest, const char *restrict src, size_t n)
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
    return dest;
}

char *strdup(const char *src)
{
    char *dst = liballoc_malloc(strlen(src) + 1);
    strcpy(dst, src);
    return dst;
}

const char *duplicate_string(const char *src, size_t len)
{
    char *dst = liballoc_malloc(len + 1);
    strncpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

s64 strtoll(const char *str, char **endptr, int base)
{
    return strntoll(str, endptr, base, strlen(str));
}

s64 strntoll(const char *str, char **endptr, int base, size_t n)
{
    s64 result = 0;
    bool negative = false;
    size_t i = 0;

    if (*str == '-')
        negative = true, str++, i++;
    else if (*str == '+')
        str++, i++;

    while (i < n && *str)
    {
        char c = *str;
        if (c >= '0' && c <= '9')
            result *= base, result += c - '0';
        else if (c >= 'a' && c <= 'z')
            result *= base, result += c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z')
            result *= base, result += c - 'A' + 10;
        else
            break;
        str++;
        i++;
    }
    if (endptr)
        *endptr = (char *) str;
    return negative ? -result : result;
}

char *strchr(const char *s, int c)
{
    while (*s)
    {
        if (*s == c)
            return (char *) s;
        s++;
    }
    return NULL;
}

size_t strspn(const char *s, const char *accept)
{
    size_t i = 0;
    while (s[i])
    {
        if (!strchr(accept, s[i]))
            break;
        i++;
    }
    return i;
}

char *strpbrk(const char *s, const char *accept)
{
    while (*s)
    {
        if (strchr(accept, *s))
            return (char *) s;
        s++;
    }
    return NULL;
}

char *strtok(char *str, const char *delim)
{
    static char *last;
    if (str == NULL)
        str = last;
    if (str == NULL)
        return NULL;
    str += strspn(str, delim);
    if (*str == '\0')
        return last = NULL;
    char *token = str;
    str = strpbrk(token, delim);
    if (str == NULL)
        last = NULL;
    else
    {
        *str = '\0';
        last = str + 1;
    }
    return token;
}

char *strtok_r(char *str, const char *delim, char **saveptr)
{
    if (str == NULL)
        str = *saveptr;

    if (str == NULL)
        return NULL;

    str += strspn(str, delim);

    if (*str == '\0')
        return *saveptr = NULL;

    char *token = str;
    str = strpbrk(token, delim);

    if (str == NULL)
        *saveptr = NULL;
    else
    {
        *str = '\0';
        *saveptr = str + 1;
    }
    return token;
}
