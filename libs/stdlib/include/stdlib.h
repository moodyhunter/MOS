// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <limits.h>
#include <mos/moslib_global.h>
#include <mos/types.h>
#include <stddef.h>

/**
 * @defgroup libs_stdlib libs.Stdlib
 * @ingroup libs
 * @brief Some standard library functions.
 * @{
 */

MOSAPI s32 abs(s32 x);
MOSAPI long labs(long x);
MOSAPI s64 llabs(s64 x);
MOSAPI s32 atoi(const char *nptr);

MOSAPI void format_size(char *buf, size_t buf_size, u64 size);

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#ifndef __MOS_KERNEL__
MOSAPI __malloc void *malloc(size_t size);
MOSAPI void free(void *ptr);
MOSAPI void *calloc(size_t nmemb, size_t size);
MOSAPI void *realloc(void *ptr, size_t size);

MOSAPI void exit(int status);
MOSAPI int atexit(void (*func)(void));
MOSAPI tid_t start_thread(const char *name, thread_entry_t entry, void *arg);
#endif

/** @} */
