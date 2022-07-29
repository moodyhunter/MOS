// SPDX-Licence-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

#define GDT_NULL    0x00
#define GDT_SEGMENT 0x10
#define GDT_PRESENT 0x80

#define GDT_CODE 0x0A // execute / read
#define GDT_DATA 0x02 // read / write
#define GDT_TSS  0x09 // execute / access

#define GDT_RING_USER   3 << 5
#define GDT_RING_KERNEL 0 << 5

#define GDT_BYTE_GRANULARITY 0x40
#define GDT_PAGE_GRANULARITY 0xC0

// Number of gdt entries
#define GDT_TABLE_SIZE 6

#define GDT_SEGMENT_NULL     0x00
#define GDT_SEGMENT_KCODE    0x08
#define GDT_SEGMENT_KDATA    0x10
#define GDT_SEGMENT_USERCODE 0x18
#define GDT_SEGMENT_USERDATA 0x20
#define GDT_SEGMENT_TSS      0x28

typedef struct
{
    u16 limit_low;
    u16 base_low;
    u8 base_middle;
    u8 access;
    u8 granularity;
    u8 base_high;
} __attr_packed gdt_entry32_t;

typedef struct
{
    u16 limit;
    gdt_entry32_t *base;
} __attr_packed gdt_ptr32_t;

static_assert(sizeof(gdt_entry32_t) == 8, "gdt_entry_t is not 8 bytes");
static_assert(sizeof(gdt_ptr32_t) == 6, "gdt_ptr_t is not 6 bytes");
