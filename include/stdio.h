// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <stdarg.h>
#include <stddef.h>

typedef struct
{
    void (*putc)(char c);
    void (*puts)(const char *s);
} char_dev_t;

// int printf(const char *format, ...);
int vsnprintf(char *buf, size_t size, const char *format, va_list args);
