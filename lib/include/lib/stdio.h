// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/mos_global.h"

#include <stdarg.h>
#include <stddef.h>

/**
 * @defgroup libs_stdio libs.Stdio
 * @ingroup libs
 * @brief Standard input/output functions.
 * @{
 */

// int printf(const char *restrict format, ...);
// int vprintf(const char *restrict format, va_list ap);

// defined in stdio.c
int __printf(2, 3) sprintf(char *restrict str, const char *restrict format, ...);
int __printf(3, 4) snprintf(char *restrict str, size_t size, const char *restrict format, ...);
int vsprintf(char *restrict str, const char *restrict format, va_list ap);

// defined in stdio_impl.c
int vsnprintf(char *restrict buf, size_t size, const char *restrict format, va_list args);

/** @} */
