// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/drivers/port.h"
#include "mos/x86/init/idt_types.h"
#include "mos/x86/interrupt/pic.h"
#include "mos/x86/x86_platform.h"

#define PIC1_OFFSET 0x20
#define PIC2_OFFSET 0x28

static idt_entry32_t idt[IDT_ENTRY_COUNT] __aligned(16) = { 0 };
static idtr32_t idtr;

static void idt_set_descriptor(uint8_t vector, void *isr, uint8_t flags)
{
    idt_entry32_t *descriptor = &idt[vector];

    descriptor->isr_low = (u32) isr & 0xFFFF;
    descriptor->kernel_code_segment = GDT_SEGMENT_KCODE; // ! GDT Kernel Code Segment
    descriptor->flags = flags;
    descriptor->isr_high = (u32) isr >> 16;
    descriptor->zero = 0;
}

void x86_idt_init()
{
    for (u8 isr = 0; isr < ISR_MAX_COUNT; isr++)
        idt_set_descriptor(isr, isr_stub_table[isr], 0x8F);

    for (u8 irq_n = 0; irq_n < IRQ_MAX_COUNT; irq_n++)
        idt_set_descriptor(irq_n + IRQ_BASE, irq_stub_table[irq_n], 0x8F);

    pic_remap_irq(PIC1_OFFSET, PIC2_OFFSET);

    idtr.base = &idt[0];
    idtr.limit = (uint16_t) sizeof(idt_entry32_t) * IDT_ENTRY_COUNT - 1;
    idt32_flush(&idtr);
}
