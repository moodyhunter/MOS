// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos_string.h"

#include <mos/moslib_global.h>
#include <mos_stdlib.h>

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

void *memcpy(void *__restrict _dst, const void *__restrict _src, size_t n)
{
    // https://github.com/eblot/newlib/blob/master/newlib/libc/string/memcpy.c

#define UNALIGNED(X, Y) (((long) X & (sizeof(long) - 1)) | ((long) Y & (sizeof(long) - 1))) // Nonzero if either X or Y is not aligned on a "long" boundary.
#define BIGBLOCKSIZE    (sizeof(long) << 2)                                                 // How many bytes are copied each iteration of the 4X unrolled loop.
#define LITTLEBLOCKSIZE (sizeof(long))                                                      // How many bytes are copied each iteration of the word copy loop.
#define TOO_SMALL(LEN)  ((LEN) < BIGBLOCKSIZE)                                              // Threshhold for punting to the byte copier.

    char *dst = _dst;
    const char *src = _src;

    /* If the size is small, or either SRC or DST is unaligned,
       then punt into the byte copy loop.  This should be rare.  */
    if (!TOO_SMALL(n) && !UNALIGNED(src, dst))
    {
        long *aligned_dst = (long *) dst;
        const long *aligned_src = (long *) src;

        /* Copy 4X long words at a time if possible.  */
        while (n >= BIGBLOCKSIZE)
        {
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            *aligned_dst++ = *aligned_src++;
            n -= BIGBLOCKSIZE;
        }

        /* Copy one long word at a time if possible.  */
        while (n >= LITTLEBLOCKSIZE)
        {
            *aligned_dst++ = *aligned_src++;
            n -= LITTLEBLOCKSIZE;
        }

        /* Pick up any residual with a byte copier.  */
        dst = (char *) aligned_dst;
        src = (char *) aligned_src;
    }

    while (n--)
        *dst++ = *src++;

    return _dst;
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

int memcmp(const void *s1, const void *s2, size_t n)
{
    const u8 *p1 = s1, *p2 = s2;
    for (size_t i = 0; i < n; i++)
    {
        if (p1[i] != p2[i])
            return p1[i] - p2[i];
    }
    return 0;
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

char *strcpy(char *__restrict dest, const char *__restrict src)
{
    while (*src)
        *dest++ = *src++;
    *dest = 0;
    return dest;
}

char *strcat(char *__restrict dest, const char *__restrict src)
{
    while (*dest)
        dest++;
    while (*src)
        *dest++ = *src++;
    *dest = 0;
    return dest;
}

char *strncpy(char *__restrict dest, const char *__restrict src, size_t n)
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
    char *dst = malloc(strlen(src) + 1);
    strcpy(dst, src);
    return dst;
}

char *strndup(const char *src, size_t len)
{
    char *dst = malloc(len + 1);
    strncpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

const char *duplicate_string(const char *src, size_t len)
{
    char *dst = malloc(len + 1);
    strncpy(dst, src, len);
    dst[len] = '\0';
    return dst;
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
