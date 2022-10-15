// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/types.h"

size_t strlen(const char *str) __pure;
s32 strcmp(const char *str1, const char *str2);
s32 strncmp(const char *str1, const char *str2, size_t n);

// ! The memory areas must not overlap.
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);
void memzero(void *s, size_t n);

void strcpy(char *dest, const char *src);
void strcat(char *dest, const char *src);
void strncpy(char *dest, const char *src, size_t n);

const char *duplicate_string_n(const char *src, size_t len);

s64 strtoll(const char *str, char **endptr, int base);
s64 strntoll(const char *str, char **endptr, int base, size_t n);

char *strchr(const char *s, int c);
