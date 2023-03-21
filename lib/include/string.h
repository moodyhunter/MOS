// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mos/lib/lib.h>
#include <mos/types.h>

/**
 * @defgroup libs_std_string libs.String
 * @ingroup libs
 * @brief String manipulation functions, similar to the ones in the C standard library.
 * @{
 */

MOSAPI size_t strlen(const char *str) __pure;
MOSAPI s32 strcmp(const char *str1, const char *str2);
MOSAPI s32 strncmp(const char *str1, const char *str2, size_t n);

// ! The memory areas must not overlap.
MOSAPI void *memcpy(void *restrict dest, const void *restrict src, size_t n);
MOSAPI void *memmove(void *dest, const void *src, size_t n);
MOSAPI void *memset(void *s, int c, size_t n);
MOSAPI void memzero(void *s, size_t n);

MOSAPI char *strcpy(char *restrict dest, const char *restrict src);
MOSAPI char *strcat(char *restrict dest, const char *restrict src);

MOSAPI char *strncpy(char *restrict dest, const char *restrict src, size_t n);

MOSAPI const char *duplicate_string(const char *src, size_t len);
MOSAPI char *strdup(const char *src);

MOSAPI s64 strtoll(const char *str, char **endptr, int base);
MOSAPI s64 strntoll(const char *str, char **endptr, int base, size_t n);

MOSAPI char *strchr(const char *s, int c);

MOSAPI size_t strspn(const char *s, const char *accept);
MOSAPI char *strpbrk(const char *s, const char *accept);
MOSAPI char *strtok(char *str, const char *delim);
MOSAPI char *strtok_r(char *str, const char *delim, char **saveptr);
/** @} */
