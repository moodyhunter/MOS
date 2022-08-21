// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "mos/mos_global.h"
#include "mos/printk.h"
#include "mos/x86/gdt/gdt_types.h"
#include "mos/x86/x86_platform.h"

static gdt_ptr32_t gdt_ptr;
static gdt_entry32_t gdt[GDT_ENTRY_COUNT] __aligned(8);

extern tss32_t tss_entry;
typedef enum
{
    GDT_ENTRY_CODE,
    GDT_ENTRY_DATA,
} gdt_entry_type_t;

typedef enum
{
    GDT_RING_KERNEL = 0,
    GDT_RING_1 = 1,
    GDT_RING_2 = 2,
    GDT_RING_USER = 3,
} gdt_ring_t;

typedef enum
{
    GDT_GRAN_BYTE = 0,
    GDT_GRAN_PAGE = 1,
} gdt_gran_t;

static gdt_entry32_t *gdt32_set_entry(u32 id, u32 base, u32 limit, gdt_entry_type_t entry_type, gdt_ring_t dpl, gdt_gran_t gran)
{
    MOS_ASSERT_X(id < GDT_ENTRY_COUNT, "GDT entry id out of bounds: %u", id);
    gdt_entry32_t *entry = &gdt[id];
    entry->base_low = MASK_BITS(base, 24);
    entry->base_high = MASK_BITS((base >> 24), 8);
    entry->limit_low = MASK_BITS(limit, 16);
    entry->limit_high = MASK_BITS((limit >> 16), 4);
    entry->present = 1;
    entry->available = 1;
    entry->read_write = true;
    entry->long_mode = false;
    entry->pm32_segment = true;
    entry->code_data_segment = true;
    entry->dpl = dpl;
    entry->executable = (entry_type == GDT_ENTRY_CODE);
    entry->granularity = gran;
    entry->accessed = false; // "Best left clear (0),"

    // ! This has to be false forever, for:
    // * 1) allow system calls to be executed in ring 0,
    // *    otherwise, it means this segment "can" be executed by all rings outer than `dpl`
    // * 2) always 0 for a TSS segment.
    entry->conforming_expand_down = false;
    return entry;
}

void x86_gdt_init()
{
    memset(gdt, 0, sizeof(gdt));

    // {Kernel,User}{Code,Data} Segments
    // We are using a flat memory model, so the base is 0 and the limit is all the way up to the end of the address space.
    gdt32_set_entry(1, 0x00000000, 0xFFFFFFFF, GDT_ENTRY_CODE, GDT_RING_KERNEL, GDT_GRAN_PAGE);
    gdt32_set_entry(2, 0x00000000, 0xFFFFFFFF, GDT_ENTRY_DATA, GDT_RING_KERNEL, GDT_GRAN_PAGE);
    gdt32_set_entry(3, 0x00000000, 0xFFFFFFFF, GDT_ENTRY_CODE, GDT_RING_USER, GDT_GRAN_PAGE);
    gdt32_set_entry(4, 0x00000000, 0xFFFFFFFF, GDT_ENTRY_DATA, GDT_RING_USER, GDT_GRAN_PAGE);

    // TSS segment
    gdt_entry32_t *tss_seg = gdt32_set_entry(5, (uintptr_t) &tss_entry, sizeof(tss_entry), GDT_ENTRY_CODE, GDT_RING_KERNEL, GDT_GRAN_BYTE);

    // ! Set special attributes for the TSS segment.
    tss_seg->code_data_segment = 0; // indicates TSS/LDT (see also `accessed`)
    tss_seg->accessed = 1;          // With a system entry (`code_data_segment` = 0), 1 indicates TSS and 0 indicates LDT
    tss_seg->read_write = 0;        // For a TSS, indicates busy (1) or not busy (0).
    tss_seg->executable = 1;        // For a TSS, 1 indicates 32-bit (1) or 16-bit (0).
    tss_seg->available = 0;         // 0 for a TSS

    gdt_ptr.base = gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt32_flush(&gdt_ptr);
}

void x86_ap_gdt_init()
{
    gdt32_flush_only(&gdt_ptr);
}
