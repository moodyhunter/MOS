// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef struct
{
    u16 isr_low;             // The lower 16 bits of the ISR's address
    u16 kernel_code_segment; // The GDT segment selector that the CPU will load into CS before calling the ISR
    u8 zero;                 // Set to zero
    u8 flags;                // Type and attributes; see the IDT page
    u16 isr_high;            // The higher 16 bits of the ISR's address
} __attr_packed idt_entry32_t;

typedef struct
{
    u16 limit;
    idt_entry32_t *base;
} __attr_packed idtr32_t;

static_assert(sizeof(idt_entry32_t) == 8, "idt_entry32_t is not 8 bytes");
static_assert(sizeof(idtr32_t) == 6, "idtr32_t is not 6 bytes");
