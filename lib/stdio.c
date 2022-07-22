// SPDX-License-Identifier: GPL-3.0-or-later

#include "stdio.h"

#include "internal/vaargs.h"

int printf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vprintf(format, args);
    va_end(args);
    return ret;
}
