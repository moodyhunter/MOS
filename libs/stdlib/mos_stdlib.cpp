// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos_stdlib.hpp"

#ifdef __MOS_KERNEL__
#include <mos/allocator.hpp>
#endif

#include <mos/types.h>
#include <mos_stdio.hpp>
#include <mos_string.hpp>

unsigned char tolower(unsigned char c)
{
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    return c;
}

static int isspace(int _c)
{
    return ((_c > 8 && _c < 14) || (_c == 32));
}

s32 abs(s32 x)
{
    return (x < 0) ? -x : x;
}

long labs(long x)
{
    return (x < 0) ? -x : x;
}

s64 llabs(s64 x)
{
    return (x < 0) ? -x : x;
}

s32 atoi(const char *nptr)
{
    s32 c;
    s32 neg = 0;
    s32 val = 0;

    while (isspace(*nptr))
        nptr++;

    if (*nptr == '-')
    {
        neg = 1;
        nptr++;
    }
    else if (*nptr == '+')
    {
        nptr++;
    }

    while ((c = *nptr++) >= '0' && c <= '9')
    {
        val = val * 10 + (c - '0');
    }

    return neg ? -val : val;
}

unsigned long strtoul(const char *__restrict nptr, char **__restrict endptr, int base)
{
    return strtoll(nptr, endptr, base);
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

void format_size(char *buf, size_t buf_size, u64 size)
{
    static const char *const units[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB" };
    size_t i = 0;
    size_t diff = 0;
    while (size >= 1024 && i < MOS_ARRAY_SIZE(units) - 1)
    {
        diff = size % 1024;
        size /= 1024;
        i++;
    }
    if (unlikely(diff == 0 || i == 0))
    {
        snprintf(buf, buf_size, "%llu %s", size, units[i]);
    }
    else
    {
        snprintf(buf, buf_size, "%llu %s + %zu %s", size, units[i], diff, units[i - 1]);
    }
}

char *string_trim(char *in)
{
    if (in == NULL)
        return NULL;

    char *end;

    // Trim leading space
    while (*in == ' ')
        in++;

    if (*in == 0) // All spaces?
        return in;

    // Trim trailing space
    end = in + strlen(in) - 1;
    while (end > in && *end == ' ')
        end--;

    // Write new null terminator
    *(end + 1) = '\0';
    return in;
}

#ifdef __MOS_KERNEL__
void *do_kmalloc(size_t size)
{
    return slab_alloc(size);
}

void *do_kcalloc(size_t nmemb, size_t size)
{
    return slab_calloc(nmemb, size);
}

void *do_krealloc(void *ptr, size_t size)
{
    return slab_realloc(ptr, size);
}

void do_kfree(const void *ptr)
{
    slab_free(ptr);
}

void *malloc(size_t size)
{
    return do_kmalloc(size);
}

void *calloc(size_t nmemb, size_t size)
{
    return do_kcalloc(nmemb, size);
}

void *realloc(void *ptr, size_t size)
{
    return do_krealloc(ptr, size);
}

void free(void *ptr)
{
    return do_kfree(ptr);
}
#endif
