// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/kernel.h"
#include "mos/x86/drivers/port.h"
#include "mos/x86/gdt_idt_tss.h"

#define IRQ_BASE 0x20

#define PIC1         0x20 /* IO base address for master PIC */
#define PIC2         0xA0 /* IO base address for slave  PIC */
#define PIC1_COMMAND PIC1
#define PIC1_DATA    (PIC1 + 1)
#define PIC2_COMMAND PIC2
#define PIC2_DATA    (PIC2 + 1)
#define PIC_EOI      0x20 /* End-of-interrupt command code */

#define PIC1_OFFSET 0x20
#define PIC2_OFFSET 0x28

__attr_aligned(16) idt_entry32_t idt[IDT_ENTRY_COUNT] = { 0 };
idtr32_t idtr;

static void idt_handle_irq(uint32_t *esp)
{
    stack_frame *stack = (stack_frame *) *esp;

    int irq = stack->intr - IRQ_BASE;

    if (irq == 7 || irq == 15)
    {
        pr_warn("IRQ %d: %s\n", irq, irq == 7 ? "Double fault" : "General protection fault");
        // these irqs may be fake ones, test it
        uint8_t pic = (irq < 8) ? PIC1 : PIC2;
        port_outb(pic + 3, 0x03);
        if ((port_inb(pic) & 0x80) != 0)
        {
            goto irq_handeled;
        }
    }

    if (irq == 1)
    {
        unsigned char scan_code = port_inb(0x60);
        pr_info("scan code: %x", scan_code);
    }

    pr_warn("handled irq: %d", irq);

irq_handeled:
    if (irq >= 8)
        port_outb(PIC2_COMMAND, PIC_EOI);
    port_outb(PIC1_COMMAND, PIC_EOI);
}

void x86_handle_interrupt(uint32_t esp)
{
    stack_frame *stack = (stack_frame *) esp;

    if (stack->intr < IRQ_BASE)
    {
        mos_panic("Unhandled exception: %d", stack->intr);
    }
    // else if (stack->intr < SYSCALL)
    // {
    //     ;
    // }
    // else if (stack->intr == SYSCALL)
    // {
    // printk("Syscall.");
    // }
    else
    {
        idt_handle_irq(&esp);
    }
}

void idt_set_descriptor(uint8_t vector, void *isr, uint8_t flags)
{
    idt_entry32_t *descriptor = &idt[vector];

    descriptor->isr_low = (u32) isr & 0xFFFF;
    descriptor->kernel_code_segment = 1 * sizeof(gdt_entry32_t); // ! GDT Kernel Code Segment
    descriptor->flags = flags;
    descriptor->isr_high = (u32) isr >> 16;
    descriptor->zero = 0;
}

void PIC_sendEOI(unsigned char irq)
{
    if (irq >= 8)
        port_outb(PIC2_COMMAND, PIC_EOI);

    port_outb(PIC1_COMMAND, PIC_EOI);
}

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

void remap_pic(int offset_master, int offset_slave)
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
        idt_set_descriptor(irq_n + IRQ_BASE, irq_stub_table[irq_n], 0x8E);

    remap_pic(PIC1_OFFSET, PIC2_OFFSET);

    idtr.base = &idt[0];
    idtr.limit = (uint16_t) sizeof(idt_entry32_t) * IDT_ENTRY_COUNT - 1;
    idt32_flush(&idtr);
}
