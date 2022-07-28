// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

typedef struct
{
    u16 limit_low;
    u16 base_low;
    u8 base_middle;
    u8 access;
    u8 granularity;
    u8 base_high;
} __attr_packed gdt_entry_t;

static_assert(sizeof(gdt_entry_t) == 8, "gdt_entry_t is not 8 bytes");

typedef struct
{
    u16 limit;
    u32 base;
} __attr_packed gdt_ptr_t;

static_assert(sizeof(gdt_ptr_t) == 6, "gdt_ptr_t is not 6 bytes");

void gdt_init();
