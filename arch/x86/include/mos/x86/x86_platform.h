// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"

static_assert(sizeof(void *) == 4, "x86_64 is not supported");

// Number of gdt entries
#define GDT_TABLE_SIZE 3

#define GDT_SEGMENT_NULL  0x00
#define GDT_SEGMENT_KCODE 0x08
#define GDT_SEGMENT_KDATA 0x10

#define PIC1         0x20 // IO base address for master PIC
#define PIC2         0xA0 // IO base address for slave  PIC
#define PIC1_COMMAND (PIC1)
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_COMMAND (PIC2)
#define PIC2_DATA    (PIC2 + 1)

typedef struct
{
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 ds, es, fs, gs;
    u32 interrupt_number;
    u32 err_code;
    u32 eip, cs, eflags;
} __attr_packed x86_stack_frame;

static_assert(sizeof(x86_stack_frame) == 68, "x86_stack_frame is not 68 bytes");
