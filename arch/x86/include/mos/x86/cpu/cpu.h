// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/types.h"
void cpu_get_msr(u32 msr, u32 *lo, u32 *hi);
void cpu_set_msr(u32 msr, u32 lo, u32 hi);

void x86_cpu_halt(void);
u32 x86_cpu_get_id(void);
