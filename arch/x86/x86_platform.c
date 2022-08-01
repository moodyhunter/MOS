// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/x86_platform.h"

#include "lib/containers.h"
#include "mos/device/console.h"
#include "mos/interrupt.h"
#include "mos/platform.h"
#include "mos/x86/drivers/port.h"
#include "mos/x86/drivers/serial.h"
#include "mos/x86/drivers/serial_console.h"
#include "mos/x86/drivers/text_mode_console.h"
#include "mos/x86/x86_init.h"
#include "mos/x86/x86_interrupt.h"

void x86_disable_interrupts()
{
    __asm__ volatile("cli");
}

void x86_enable_interrupts()
{
    __asm__ volatile("sti");
}

void x86_install_interrupt_handler(u32 irq, irq_handler_descriptor_t *handler)
{
    list_node_append(&irq_handlers[irq], list_node(handler));
}

serial_console_t com1_console = {
    .device = { .port = COM1, .baud_rate = 115200, .char_length = CHAR_LENGTH_8, .stop_bits = STOP_BITS_1, .parity = PARITY_EVEN },
    .console = { .name = "Serial Console for COM1", .caps = CONSOLE_CAP_SETUP, .setup = serial_console_setup },
};

// !! TODO: Use kmalloc to allocate the handler_descriptor
irq_handler_descriptor_t com1_handler = {
    .list_node = MOS_LIST_NODE_INIT(com1_handler),
    .handler = serial_irq_handler,
};

void x86_init()
{
    x86_disable_interrupts();

    x86_gdt_init();
    x86_idt_init();
    x86_tss_init();
    x86_irq_handler_init();

    // prevent getting the timer interrupt
    irq_mask(IRQ_TIMER);
    irq_unmask(IRQ_KEYBOARD);
    irq_unmask(IRQ_COM1);
    irq_unmask(IRQ_COM2);
    x86_install_interrupt_handler(IRQ_COM1, &com1_handler);

    register_console(&com1_console.console);
    register_console(&vga_text_mode_console);
}

void __attr_noreturn x86_shutdown_vm()
{
    x86_disable_interrupts();
    port_outw(0x604, 0x2000);
    while (1)
        ;
}

mos_platform_t mos_platform = {
    .platform_init = x86_init,
    .platform_shutdown = x86_shutdown_vm,
    .disable_interrupts = x86_disable_interrupts,
    .enable_interrupts = x86_enable_interrupts,
    .install_irq_handler = x86_install_interrupt_handler,
};
