// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

typedef struct
{
    uint16_t isr_low;   // The lower 16 bits of the ISR's address
    uint16_t kernel_cs; // The GDT segment selector that the CPU will load into CS before calling the ISR
    uint8_t reserved;   // Set to zero
    uint8_t attributes; // Type and attributes; see the IDT page
    uint16_t isr_high;  // The higher 16 bits of the ISR's address
} __attr_packed idt_entry32_t;

static_assert(sizeof(idt_entry32_t) == 8, "idt_entry32_t is not 8 bytes");

typedef struct
{
    u16 limit;
    u32 base;
} __attr_packed idtr32_t;

static_assert(sizeof(idtr32_t) == 6, "idtr32_t is not 6 bytes");

void idt_init();
