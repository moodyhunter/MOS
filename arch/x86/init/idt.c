// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/drivers/port.h"
#include "mos/x86/x86_init.h"
#include "mos/x86/x86_platform.h"

// Reinitialize the PIC controllers.
// Giving them specified vector offsets rather than 8h and 70h, as configured by default
// ICW: Initialization command words
#define ICW1_ICW4      0x01 /* ICW4 (not) needed */
#define ICW1_SINGLE    0x02 /* Single (cascade) mode */
#define ICW1_INTERVAL4 0x04 /* Call address interval 4 (8) */
#define ICW1_LEVEL     0x08 /* Level triggered (edge) mode */
#define ICW1_INIT      0x10 /* Initialization - required! */

#define ICW4_8086       0x01 /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO       0x02 /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE  0x08 /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C /* Buffered mode/master */
#define ICW4_SFNM       0x10 /* Special fully nested (not) */

#define PIC1_OFFSET 0x20
#define PIC2_OFFSET 0x28

__attr_aligned(16) idt_entry32_t idt[IDT_ENTRY_COUNT] = { 0 };
idtr32_t idtr;

static void idt_set_descriptor(uint8_t vector, void *isr, uint8_t flags)
{
    idt_entry32_t *descriptor = &idt[vector];

    descriptor->isr_low = (u32) isr & 0xFFFF;
    descriptor->kernel_code_segment = GDT_SEGMENT_KCODE; // ! GDT Kernel Code Segment
    descriptor->flags = flags;
    descriptor->isr_high = (u32) isr >> 16;
    descriptor->zero = 0;
}

static void remap_pic(int offset_master, int offset_slave)
{
    u8 a1, a2;

    a1 = port_inb(PIC1_DATA); // save masks
    a2 = port_inb(PIC2_DATA);

    port_outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4); // starts the initialization sequence (in cascade mode)
    port_outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    port_outb(PIC1_DATA, offset_master); // ICW2: Master PIC vector offset
    port_outb(PIC2_DATA, offset_slave);  // ICW2: Slave PIC vector offset

    port_outb(PIC1_DATA, 4); // ICW3: tell Master PIC that there is a slave PIC at IRQ2 (0000 0100)
    port_outb(PIC2_DATA, 2); // ICW3: tell Slave PIC its cascade identity (0000 0010)

    port_outb(PIC1_DATA, ICW4_8086);
    port_outb(PIC2_DATA, ICW4_8086);

    port_outb(PIC1_DATA, a1); // restore saved masks.
    port_outb(PIC2_DATA, a2);
}

void x86_idt_init()
{
    for (u8 isr = 0; isr < ISR_MAX_COUNT; isr++)
        idt_set_descriptor(isr, isr_stub_table[isr], 0x8F);

    for (u8 irq_n = 0; irq_n < IRQ_MAX_COUNT; irq_n++)
        idt_set_descriptor(irq_n + IRQ_BASE, irq_stub_table[irq_n], 0x8F);

    remap_pic(PIC1_OFFSET, PIC2_OFFSET);

    idtr.base = &idt[0];
    idtr.limit = (uint16_t) sizeof(idt_entry32_t) * IDT_ENTRY_COUNT - 1;
    idt32_flush(&idtr);
}
