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

MOSAPI s64 strtoll(const char *str, char **endptr, int base);
MOSAPI s64 strntoll(const char *str, char **endptr, int base, size_t n);

MOSAPI void format_size(char *buf, size_t buf_size, u64 size);
MOSAPI char *string_trim(char *in);

// clang-format off
#define MIN(a, b) __extension__ ({ __extension__ __auto_type _a = (a); __auto_type _b = (b); _a < _b ? _a : _b; })
#define MAX(a, b) __extension__ ({ __extension__ __auto_type _a = (a); __auto_type _b = (b); _a > _b ? _a : _b; })
#define pow2(x) ((__typeof__(x)) 1 << (x))
// clang-format on

// malloc, free, calloc and realloc
#ifndef __MOS_KERNEL__
MOSAPI __malloc void *malloc(size_t size);
MOSAPI void free(void *ptr);
MOSAPI void *calloc(size_t nmemb, size_t size);
MOSAPI void *realloc(void *ptr, size_t size);
MOSAPI pid_t spawn(const char *path, const char *const argv[]);
#else
#include "mos/mm/slab.h"

#define kmalloc(item) _Generic((item), slab_t *: kmemcache_alloc, default: slab_alloc)(item)

should_inline __malloc void *kcalloc(size_t nmemb, size_t size)
{
    return slab_calloc(nmemb, size);
}

should_inline void *krealloc(void *ptr, size_t size)
{
    return slab_realloc(ptr, size);
}

should_inline void kfree(const void *ptr)
{
    slab_free(ptr);
}

#ifdef __IN_MOS_LIBS__
#define malloc(size)        slab_alloc(size)
#define calloc(nmemb, size) slab_calloc(nmemb, size)
#define realloc(ptr, size)  slab_realloc(ptr, size)
#define free(ptr)           slab_free(ptr)
#endif

#endif

#ifndef __MOS_KERNEL__
MOSAPI noreturn void exit(int status);
MOSAPI int atexit(void (*func)(void));
MOSAPI tid_t start_thread(const char *name, thread_entry_t entry, void *arg);
MOSAPI noreturn void abort(void);
#endif

/** @} */
