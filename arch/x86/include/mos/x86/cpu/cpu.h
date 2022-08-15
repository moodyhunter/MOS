// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

void cpu_get_msr(u32 msr, u32 *lo, u32 *hi);
void cpu_set_msr(u32 msr, u32 lo, u32 hi);

void x86_cpu_halt();
