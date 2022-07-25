// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/stdlib.h"

#include "mos/drivers/screen.h"
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
