// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <mos/types.h>

/**
 * @defgroup libs_std_string libs.String
 * @ingroup libs
 * @brief String manipulation functions, similar to the ones in the C standard library.
 * @{
 */

MOSAPI size_t strlen(const char *str) __pure;
MOSAPI size_t strnlen(const char *, size_t);
MOSAPI s32 strcmp(const char *str1, const char *str2);
MOSAPI s32 strncmp(const char *str1, const char *str2, size_t n);
MOSAPI s32 strncasecmp(const char *str1, const char *str2, size_t n);

// ! The memory areas must not overlap.
MOSAPI void *memcpy(void *__restrict dest, const void *__restrict src, size_t n);
MOSAPI void *memmove(void *dest, const void *src, size_t n);
MOSAPI void *memset(void *s, int c, size_t n);
MOSAPI int memcmp(const void *s1, const void *s2, size_t n);
MOSAPI void memzero(void *s, size_t n);
MOSAPI void *memchr(const void *m, int c, size_t n);

MOSAPI char *strcpy(char *__restrict dest, const char *__restrict src);
MOSAPI char *strcat(char *__restrict dest, const char *__restrict src);

MOSAPI char *strncpy(char *__restrict dest, const char *__restrict src, size_t n);

MOSAPI char *strdup(const char *src);
MOSAPI char *strndup(const char *src, size_t n);

MOSAPI char *strchr(const char *s, int c);
MOSAPI char *strrchr(const char *s, int c);

MOSAPI size_t strspn(const char *s, const char *accept);
MOSAPI char *strpbrk(const char *s, const char *accept);
MOSAPI char *strtok(char *str, const char *delim);
MOSAPI char *strtok_r(char *str, const char *delim, char **saveptr);
/** @} */
