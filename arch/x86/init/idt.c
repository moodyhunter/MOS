// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/idt.h"

#include "mos/kernel.h"

#define IDT_MAX_DESCRIPTORS 256
__attr_aligned(16) static idt_entry32_t idt[IDT_MAX_DESCRIPTORS];
static bool vectors[IDT_MAX_DESCRIPTORS];
static idtr32_t idtr;

struct interrupt_frame
{
    u16 ip;
    u16 cs;
    u16 flags;
    u16 sp;
    u16 ss;
};

__attr_noreturn void exception_handler();

extern void *isr_stub_table[];

void exception_handler()
{
    pr_warn("exception");

    // // Completely hangs the computer
    // __asm__ volatile("cli");
    // __asm__ volatile("hlt");
    while (1)
        ;
}
void idt_set_descriptor(uint8_t vector, void *isr, uint8_t flags)
{
    idt_entry32_t *descriptor = &idt[vector];

    descriptor->isr_low = (uint32_t) isr & 0xFFFF;
    descriptor->kernel_cs = 0x08; // this value can be whatever offset your kernel code selector is in your GDT
    descriptor->attributes = flags;
    descriptor->isr_high = (uint32_t) isr >> 16;
    descriptor->reserved = 0;
}

void idt_init()
{
    idtr.base = (uintptr_t) &idt[0];
    idtr.limit = (uint16_t) sizeof(idt_entry32_t) * IDT_MAX_DESCRIPTORS - 1;

    for (uint8_t vector = 0; vector < 32; vector++)
    {
        idt_set_descriptor(vector, isr_stub_table[vector], 0x8E);
        vectors[vector] = true;
    }

    __asm__ volatile("lidt %0" : : "m"(idtr)); // load the new IDT
    __asm__ volatile("sti");                   // set the interrupt flag
}
