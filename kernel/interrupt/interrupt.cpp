// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/interrupt/interrupt.hpp"

#include "mos/lib/structures/list.hpp"

#include <mos/allocator.hpp>
#include <mos_stdlib.hpp>

struct interrupt_handler_t : mos::NamedType<"InterruptHandler">
{
    as_linked_list;
    u32 irq;
    irq_serve_t handler;
    void *data;
};

static spinlock_t irq_handlers_lock;
static list_head irq_handlers;

void interrupt_entry(u32 irq)
{
    // spinlock_acquire(&irq_handlers_lock);

    list_foreach(interrupt_handler_t, handler, irq_handlers)
    {
        if (handler->irq == irq)
            if (handler->handler(irq, handler->data))
                break; // interrupt was handled
    }

    // spinlock_release(&irq_handlers_lock);
}

void interrupt_handler_register(u32 irq, irq_serve_t handler, void *data)
{
    interrupt_handler_t *new_handler = mos::create<interrupt_handler_t>();
    new_handler->irq = irq;
    new_handler->handler = handler;
    new_handler->data = data;

    spinlock_acquire(&irq_handlers_lock);
    list_node_append(&irq_handlers, &new_handler->list_node);
    spinlock_release(&irq_handlers_lock);
}
