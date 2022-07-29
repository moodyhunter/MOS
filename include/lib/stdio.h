// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/mos_global.h"

#include <stdarg.h>
#include <stddef.h>

// int printf(const char *restrict format, ...);
// int vprintf(const char *restrict format, va_list ap);

// defined in stdio.c
int __attr_printf(2, 3) sprintf(char *restrict str, const char *restrict format, ...);
int __attr_printf(3, 4) snprintf(char *restrict str, size_t size, const char *restrict format, ...);
int vsprintf(char *restrict str, const char *restrict format, va_list ap);

int tttttt(int c);

// defined in stdio_impl.c
int vsnprintf(char *restrict buf, size_t size, const char *restrict format, va_list args);
