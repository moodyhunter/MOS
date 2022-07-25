// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/math.h"
#include "mos/types.h"

#include <stddef.h>
#include <stdnoreturn.h>

#define MOS_UNUSED(x) (void) (x)

noreturn void kpanic(const char *msg, const char *source_loc);

int isspace(int c);
s32 atoi(const char *nptr);
