// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/types.h"

#define GDT_NULL    0x00
#define GDT_SEGMENT 0x10
#define GDT_PRESENT 0x80

#define GDT_GRANULARITY_BYTE 0x40
#define GDT_GRANULARITY_PAGE 0xC0

#define GDT_SEGMENT_NULL     0x00
#define GDT_SEGMENT_KCODE    0x08
#define GDT_SEGMENT_KDATA    0x10
#define GDT_SEGMENT_USERCODE 0x18
#define GDT_SEGMENT_USERDATA 0x20
#define GDT_SEGMENT_TSS      0x28
#define GDT_ENTRY_COUNT      6

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

typedef struct
{
    u32 link;
    u32 esp0, ss0;
    u32 esp1, ss1;
    u32 esp2, ss2;

    u32 cr3;
    u32 eip;
    u32 eflags;

    u32 eax, ecx, edx, ebx, esp, ebp, esi, edi;
    u32 es, cs, ss, ds, fs, gs;

    u32 ldtr;
    u16 trap;
    u16 iomap;
} __packed tss32_t;

static_assert(sizeof(tss32_t) == 104, "tss32_t is not 104 bytes");

typedef struct
{
    tss32_t tss __aligned(8);
    gdt_entry32_t gdt[GDT_ENTRY_COUNT] __aligned(8);
    gdt_ptr32_t gdt_ptr __aligned(8);
} __packed x86_cpu_descriptor_t;

extern PER_CPU_DECLARE(x86_cpu_descriptor_t, x86_cpu_descriptor);

void x86_init_current_cpu_gdt();
void x86_init_current_cpu_tss();

// The following 5 symbols are defined in the descriptor_flush.asm file.
extern asmlinkage void gdt32_flush(gdt_ptr32_t *gdt_ptr);
extern asmlinkage void tss32_flush(u32 tss_selector);
extern asmlinkage void gdt32_flush_only(gdt_ptr32_t *gdt_ptr);
