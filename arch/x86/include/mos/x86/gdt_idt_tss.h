// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mos_global.h"
#include "mos/types.h"
#include "mos/x86/gdt_types.h"
#include "mos/x86/idt_types.h"
#include "mos/x86/tss_types.h"

typedef struct
{
    u32 edi, esi, ebp, esp, ebx, edx, ecx, eax;
    u32 ds, es, fs, gs;

    u32 intr;
    u32 errc;

    u32 eip;
    u32 cs;
    u32 eflags;
    u32 u_esp;
    u32 u_ss;
} __attr_packed stack_frame;

extern gdt_ptr32_t gdt_ptr;
extern gdt_entry32_t gdt[GDT_TABLE_SIZE];
extern tss32_t tss;
extern idtr32_t idtr;
extern idt_entry32_t idt[IDT_ENTRY_COUNT];

// The following 3 symbols are defined in the gdt_tss_idt.asm file.
extern void gdt32_flush(gdt_ptr32_t *gdt_ptr);
extern void tss32_flush(u32 tss_selector);
extern void idt32_flush(idtr32_t *idtr);

// The following 2 symbols are defined in the interrupt_handler.asm file.
extern void *isr_stub_table[];
extern void *irq_stub_table[];
