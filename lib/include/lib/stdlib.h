// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

#include <limits.h>
#include <stddef.h>

s32 abs(s32 x);
long labs(long x);
s64 llabs(s64 x);
s32 atoi(const char *nptr);

void format_size(char *buf, size_t buf_size, u64 size);

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
