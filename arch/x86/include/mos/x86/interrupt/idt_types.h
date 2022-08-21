// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef struct
{
    u16 isr_low; // The lower 16 bits of the ISR's address
    u16 segment; // The GDT segment selector that the CPU will load into CS before calling the ISR
    u32 args : 5;
    u32 reserved : 3;
    u32 type : 4; // The type of interrupt
    u32 s : 1;
    u32 dpl : 2;
    u32 present : 1;
    u32 isr_high : 16; // The upper 16 bits of the ISR's address
} __packed idt_entry32_t;

typedef struct
{
    u16 limit;
    idt_entry32_t *base;
} __packed idtr32_t;

static_assert(sizeof(idt_entry32_t) == 8, "idt_entry32_t is not 8 bytes");
static_assert(sizeof(idtr32_t) == 6, "idtr32_t is not 6 bytes");
