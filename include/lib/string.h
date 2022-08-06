// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/types.h"

// Inclusion of the <string.h> header may also make visible all symbols from <stddef.h>.
#include <stddef.h>

size_t strlen(const char *str);
s32 strcmp(const char *str1, const char *str2);

// ! The memory areas must not overlap.
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t n);

void strcpy(char *dest, const char *src);
void strncpy(char *dest, const char *src, size_t n);
