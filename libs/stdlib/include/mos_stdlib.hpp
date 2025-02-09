// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/tasks/signal_types.h>
#include <mos/types.h>
#include <stddef.h>

/**
 * @defgroup libs_stdlib libs.Stdlib
 * @ingroup libs
 * @brief Some standard library functions.
 * @{
 */

MOSAPI unsigned char tolower(unsigned char c);
MOSAPI s32 abs(s32 x);
MOSAPI long labs(long x);
MOSAPI s64 llabs(s64 x);
MOSAPI s32 atoi(const char *nptr);

MOSAPI unsigned long strtoul(const char *nptr, char **endptr, int base);
MOSAPI s64 strtoll(const char *str, char **endptr, int base);
MOSAPI s64 strntoll(const char *str, char **endptr, int base, size_t n);

MOSAPI void format_size(char *buf, size_t buf_size, u64 size);
MOSAPI char *string_trim(char *in);

// clang-format off
#define MIN(a, b) __extension__ ({ const auto _a = (a); __auto_type _b = (b); _a < _b ? _a : _b; })
#define MAX(a, b) __extension__ ({ const auto _a = (a); __auto_type _b = (b); _a > _b ? _a : _b; })
// clang-format on

#ifdef __MOS_KERNEL__
#ifdef __cplusplus
typedef struct _slab slab_t;
void *kmalloc(size_t size);
void *kmalloc(slab_t *slab);
__malloc void *kcalloc(size_t nmemb, size_t size);
void *krealloc(void *ptr, size_t size);
void kfree(const void *ptr);
#endif

#ifdef __IN_MOS_LIBS__
MOSAPI void *malloc(size_t size);
MOSAPI void *calloc(size_t nmemb, size_t *size);
MOSAPI void *realloc(void *ptr, size_t size);
MOSAPI void free(void *ptr);
#endif

#else
// malloc, free, calloc and realloc should be provided by libc
MOSAPI pid_t spawn(const char *path, const char *const argv[]);
#endif

#ifndef __MOS_KERNEL__
MOSAPI void exit(int status) __attribute__((noreturn));
MOSAPI int atexit(void (*func)(void));
MOSAPI tid_t start_thread(const char *name, thread_entry_t entry, void *arg);
MOSAPI void abort(void) __attribute__((noreturn));
#endif

/** @} */
