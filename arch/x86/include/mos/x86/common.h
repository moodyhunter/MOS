// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

static_assert(sizeof(void *) == 4, "x86_64 is not supported");

void x86_gdt_init();
void x86_tss_init();
void x86_idt_init();
