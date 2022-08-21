// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

#define GDT_NULL    0x00
#define GDT_SEGMENT 0x10
#define GDT_PRESENT 0x80

#define GDT_GRANULARITY_BYTE 0x40
#define GDT_GRANULARITY_PAGE 0xC0

typedef struct
{
    u32 limit_low : 16;
    u32 base_low : 24;
    u32 accessed : 1;
    u32 read_write : 1;             // readable for code, writable for data
    u32 conforming_expand_down : 1; // conforming for code, expand down for data
    u32 executable : 1;             // 1 for code, 0 for data
    u32 code_data_segment : 1;      // should be 1 for everything but TSS and LDT
    u32 dpl : 2;                    // privilege level
    u32 present : 1;
    u32 limit_high : 4;
    u32 available : 1; // only used in software; has no effect on hardware
    u32 long_mode : 1;
    u32 pm32_segment : 1; // 32-bit opcodes for code, uint32_t stack for data
    u32 granularity : 1;  // 1 to use 4k page addressing, 0 for byte addressing
    u32 base_high : 8;
} __packed gdt_entry32_t;

typedef struct
{
    u16 limit;
    gdt_entry32_t *base;
} __packed gdt_ptr32_t;

static_assert(sizeof(gdt_entry32_t) == 8, "gdt_entry_t is not 8 bytes");
static_assert(sizeof(gdt_ptr32_t) == 6, "gdt_ptr_t is not 6 bytes");
