// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/interrupt/idt.h"

#include "mos/x86/descriptors/descriptor_types.h"
#include "mos/x86/x86_interrupt.h"
#include "mos/x86/x86_platform.h"

static idt_entry32_t idt[IDT_ENTRY_COUNT] __aligned(16) = { 0 };
static idtr32_t idtr;

#define IDT_FLAG_P (1 << 7)

#define STS_IG32 0xE // 32-bit Interrupt Gate
#define STS_TG32 0xF // 32-bit Trap Gate

extern asmlinkage void idt32_flush(idtr32_t *idtr);

static void idt_set_descriptor(u8 vector, void *isr, bool usermode, bool is_trap)
{
    idt_entry32_t *descriptor = &idt[vector];

    descriptor->isr_low = (u32) isr & 0xFFFF;
    descriptor->isr_high = (u32) isr >> 16;
    descriptor->segment = GDT_SEGMENT_KCODE;
    descriptor->present = true;
    descriptor->dpl = usermode ? 3 : 0;
    descriptor->type = is_trap ? STS_TG32 : STS_IG32;
    descriptor->s = 0;
    descriptor->reserved = 0;
    descriptor->args = 0;
}

void x86_idt_flush()
{
    idt32_flush(&idtr);
}

void x86_idt_init()
{
    for (u8 isr = 0; isr < ISR_MAX_COUNT; isr++)
        idt_set_descriptor(isr, isr_stub_table[isr], false, false);

    for (u8 irq_n = 0; irq_n < IRQ_MAX_COUNT; irq_n++)
        idt_set_descriptor(irq_n + IRQ_BASE, irq_stub_table[irq_n], false, false);

    // system calls
    idt_set_descriptor(MOS_SYSCALL_INTR, isr_stub_table[MOS_SYSCALL_INTR], true, true);

    idtr.base = &idt[0];
    idtr.limit = (u16) sizeof(idt_entry32_t) * IDT_ENTRY_COUNT - 1;
    idt32_flush(&idtr);
}
