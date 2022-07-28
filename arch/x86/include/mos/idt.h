// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

typedef struct idt_entry_t
{
    u16 base_lo;
    u16 seg_sel;
    u8 always0;
    u8 flags;
    u16 base_hi;
} __attr_packed idt_entry_t;
