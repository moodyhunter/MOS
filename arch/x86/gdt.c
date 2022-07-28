// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/gdt.h"

// stolen from https://github.com/knusbaum/kernel/blob/b596b33853c5ef88e4c6fb7d25ae9221ae192ef8/gdt.c#L66
static void gdt_set_gate(gdt_entry_t *entry, uint32_t base, uint32_t limit, uint8_t access, uint8_t granularity)
{
    entry->base_low = (base & 0xFFFF);
    entry->base_middle = (base >> 16) & 0xFF;
    entry->base_high = (base >> 24) & 0xFF;

    entry->limit_low = (limit & 0xFFFF);
    entry->granularity = (limit >> 16) & 0x0F;

    entry->granularity |= granularity & 0xF0;
    entry->access = access;
}

// defined in gdt_flush.S
extern void x86_gdt_flush(u32 gdt_ptr);
void gdt_init()
{
    // Disable interrupts
    __asm__ volatile("cli");
    static gdt_entry_t gdt[6];
    static gdt_ptr_t gdt_ptr;

    gdt_set_gate(&gdt[0], 0, 0, 0, 0);                // Null segment
    gdt_set_gate(&gdt[1], 0, 0xFFFFFFFF, 0x9A, 0xCF); // Code segment
    gdt_set_gate(&gdt[2], 0, 0xFFFFFFFF, 0x92, 0xCF); // Data segment
    gdt_set_gate(&gdt[3], 0, 0xFFFFFFFF, 0xFA, 0xCF); // User mode code segment
    gdt_set_gate(&gdt[4], 0, 0xFFFFFFFF, 0xF2, 0xCF); // User mode data segment

    // ! TODO: a task segment is not implemented yet

    gdt_ptr.base = (uint32_t) &gdt;
    gdt_ptr.limit = sizeof(gdt) - 1;
    x86_gdt_flush((uint32_t) &gdt_ptr);
}
