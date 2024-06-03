// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/descriptors/descriptors.h"

#include <mos/platform/platform.h>
#include <mos/syslog/printk.h>
#include <mos/x86/x86_platform.h>
#include <mos_string.h>

typeof(x86_cpu_descriptor) x86_cpu_descriptor = { 0 };

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

static gdt_entry_t *gdt_set_entry(gdt_entry_t *entry, ptr_t base, u32 limit, gdt_entry_type_t entry_type, gdt_ring_t dpl, gdt_gran_t gran)
{
    entry->base_low = MASK_BITS(base, 24);
    entry->base_high = MASK_BITS((base >> 24), 8);
    entry->base_veryhigh = base >> 32;
    entry->long_mode_code = entry_type == GDT_ENTRY_CODE;
    entry->pm32_segment = !entry->long_mode_code; // it's one or the other

    entry->limit_low = MASK_BITS(limit, 16);
    entry->limit_high = MASK_BITS((limit >> 16), 4);
    entry->present = 1;
    entry->available = 1;
    entry->read_write = true;
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

void x86_init_percpu_gdt()
{
    x86_cpu_descriptor_t *this_cpu_desc = per_cpu(x86_cpu_descriptor);
    memzero(this_cpu_desc, sizeof(x86_cpu_descriptor_t));

    // {Kernel,User}{Code,Data} Segments
    // We are using a flat memory model, so the base is 0 and the limit is all the way up to the end of the address space.
    gdt_set_entry(&this_cpu_desc->gdt[1], 0x00000000, 0xFFFFFFFF, GDT_ENTRY_CODE, GDT_RING_KERNEL, GDT_GRAN_PAGE);
    gdt_set_entry(&this_cpu_desc->gdt[2], 0x00000000, 0xFFFFFFFF, GDT_ENTRY_DATA, GDT_RING_KERNEL, GDT_GRAN_PAGE);
    gdt_set_entry(&this_cpu_desc->gdt[3], 0x00000000, 0xFFFFFFFF, GDT_ENTRY_CODE, GDT_RING_USER, GDT_GRAN_PAGE);
    gdt_set_entry(&this_cpu_desc->gdt[4], 0x00000000, 0xFFFFFFFF, GDT_ENTRY_DATA, GDT_RING_USER, GDT_GRAN_PAGE);

    // TSS segment
    gdt_entry_t *tss_seg = gdt_set_entry(&this_cpu_desc->gdt[5], (ptr_t) &this_cpu_desc->tss, sizeof(tss64_t), GDT_ENTRY_CODE, GDT_RING_KERNEL, GDT_GRAN_BYTE);

    // ! Set special attributes for the TSS segment.
    tss_seg->code_data_segment = 0; // indicates TSS/LDT (see also `accessed`)
    tss_seg->accessed = 1;          // With a system entry (`code_data_segment` = 0), 1 indicates TSS and 0 indicates LDT
    tss_seg->read_write = 0;        // For a TSS, indicates busy (1) or not busy (0).
    tss_seg->executable = 1;        // For a TSS, 1 indicates 32-bit (1) or 16-bit (0).
    tss_seg->available = 0;         // 0 for a TSS

    this_cpu_desc->gdt_ptr.base = this_cpu_desc->gdt;
    this_cpu_desc->gdt_ptr.limit = sizeof(this_cpu_desc->gdt) - 1;
    gdt_flush(&this_cpu_desc->gdt_ptr);
}

void x86_init_percpu_tss()
{
    x86_cpu_descriptor_t *this_cpu_desc = per_cpu(x86_cpu_descriptor);
    memzero(&this_cpu_desc->tss, sizeof(tss64_t));
    tss_flush(GDT_SEGMENT_TSS);
}
