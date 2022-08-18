// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/printk.h"
#include "mos/x86/gdt/gdt_types.h"
#include "mos/x86/x86_platform.h"

static gdt_ptr32_t gdt_ptr;
static gdt_entry32_t gdt[GDT_ENTRY_COUNT] __aligned(8);

// Stolen from https://github.com/szhou42/osdev/blob/52c02f0d4327442493459253a5c6c83c5f378765/src/kernel/descriptor_tables/gdt.c#L33
// Originally licensed under the GPLv3 license.
static void gdt32_set_entry(u32 id, u32 base, u32 limit, u8 access, u8 granularity)
{
    MOS_ASSERT_X(id < GDT_ENTRY_COUNT, "GDT entry id out of bounds: %u", id);
    gdt_entry32_t *entry = &gdt[id];
    entry->base_low = base & 0xFFFF;
    entry->base_middle = (base >> 16) & 0xFF;
    entry->base_high = (base >> 24) & 0xFF;

    // The limit field is in both 'limit_low' and 'granularity'
    // WTF is this design???
    entry->limit_low = limit & 0xFFFF;
    entry->granularity = (limit >> 16) & 0xF;

    entry->access = access;

    // "Only need the high 4 bits of gran"
    entry->granularity |= (granularity & 0xF0);
}

void x86_gdt_init()
{
    gdt32_set_entry(0, 0, 0, 0, GDT_GRANULARITY_PAGE);

    // {Kernel,User}{Code,Data} Segments
    // We are using a flat memory model, so the base is 0 and the limit is all the way up to the end of the address space.
    gdt32_set_entry(1, 0x00000000, 0xFFFFFFFF, GDT_PRESENT | GDT_SEGMENT | GDT_CODE | GDT_RING_KERNEL, GDT_GRANULARITY_PAGE);
    gdt32_set_entry(2, 0x00000000, 0xFFFFFFFF, GDT_PRESENT | GDT_SEGMENT | GDT_DATA | GDT_RING_KERNEL, GDT_GRANULARITY_PAGE);
    gdt32_set_entry(3, 0x00000000, 0xFFFFFFFF, GDT_PRESENT | GDT_SEGMENT | GDT_CODE | GDT_RING_USER, GDT_GRANULARITY_PAGE);
    gdt32_set_entry(4, 0x00000000, 0xFFFFFFFF, GDT_PRESENT | GDT_SEGMENT | GDT_DATA | GDT_RING_USER, GDT_GRANULARITY_PAGE);

    gdt_ptr.base = gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt32_flush(&gdt_ptr);
}

void x86_ap_gdt_init()
{
    gdt32_flush_only(&gdt_ptr);
}
