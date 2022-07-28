// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/math.h"
#include "mos/mos_global.h"
#include "mos/types.h"

#include <limits.h>
#include <stddef.h>

__attr_noreturn void kpanic(const char *msg, const char *source_loc);

s32 abs(s32 x);
long labs(long x);
s64 llabs(s64 x);

s32 atoi(const char *nptr);
