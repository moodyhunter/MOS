// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mos/mos_global.h>
#include <mos/moslib_global.h>
#include <stdarg.h>
#include <stddef.h>

/**
 * @defgroup libs_stdio libs.Stdio
 * @ingroup libs
 * @brief Standard input/output functions.
 * @{
 */

#ifndef __MOS_KERNEL__ // for userspace only
MOSAPI int __printf(1, 2) printf(const char *restrict format, ...);
#endif

// defined in stdio.c
MOSAPI int __printf(2, 3) sprintf(char *restrict str, const char *restrict format, ...);
MOSAPI int __printf(3, 4) snprintf(char *restrict str, size_t size, const char *restrict format, ...);
MOSAPI int vsprintf(char *restrict str, const char *restrict format, va_list ap);

// defined in stdio_impl.c
MOSAPI int vsnprintf(char *restrict buf, size_t size, const char *restrict format, va_list args);

#ifndef __MOS_KERNEL__ // for userspace only
MOSAPI int getchar(void);
MOSAPI int putchar(int c);
MOSAPI int puts(const char *s);
#endif

/** @} */
