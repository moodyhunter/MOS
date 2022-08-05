// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/stdlib.h"

#include "lib/stdio.h"
#include "mos/types.h"

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

void format_size(char *buf, size_t buf_size, u64 size)
{
    if (size < 1024)
        snprintf(buf, buf_size, "%llu B", size);
    else if (size < 1024 * 1024)
        snprintf(buf, buf_size, "%llu KiB", size / 1024);
    else if (size < 1024 * 1024 * 1024)
        snprintf(buf, buf_size, "%llu MiB", size / (1024 * 1024));
    else
        snprintf(buf, buf_size, "%llu GiB", size / (1024 * 1024 * 1024));
}
