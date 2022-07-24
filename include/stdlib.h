// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "types.h"

#include <stdnoreturn.h>

#define MOS_UNUSED(x) (void) (x)

noreturn void kpanic(const char *msg, const char *source_loc);

bool isspace(uchar c);
s32 atoi(const char *nptr);
