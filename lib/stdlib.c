// SPDX-License-Identifier: GPL-3.0-or-later

#include "stdlib.h"

#include "drivers/screen.h"
#include "types.h"

bool isspace(unsigned char _c)
{
    return ((_c > 8 && _c < 14) || (_c == 32));
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
