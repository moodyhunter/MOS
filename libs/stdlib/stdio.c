// SPDX-License-Identifier: GPL-3.0-or-later

#include <limits.h>
#include <stdio.h>

int sprintf(char *__restrict str, const char *__restrict format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vsprintf(str, format, args);
    va_end(args);
    return ret;
}

int snprintf(char *__restrict str, size_t size, const char *__restrict format, ...)
{
    va_list args;
    va_start(args, format);
    int ret = vsnprintf(str, size, format, args);
    va_end(args);
    return ret;
}

int vsprintf(char *__restrict str, const char *__restrict format, va_list ap)
{
    return vsnprintf(str, INT_MAX, format, ap);
}
