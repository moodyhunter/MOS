// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/interrupt/interrupt.h"

#include "mos/lib/structures/list.h"
#include "mos/mm/slab_autoinit.h"

#include <mos_stdlib.h>

typedef struct
{
    as_linked_list;
    u32 irq;
    irq_serve_t handler;
    void *data;
} interrupt_handler_t;

static spinlock_t irq_handlers_lock = SPINLOCK_INIT;
static list_head irq_handlers = LIST_HEAD_INIT(irq_handlers);

static slab_t *irq_handler_slab = NULL;
SLAB_AUTOINIT("irq_handler", irq_handler_slab, interrupt_handler_t);

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
    interrupt_handler_t *new_handler = kmalloc(irq_handler_slab);
    new_handler->irq = irq;
    new_handler->handler = handler;
    new_handler->data = data;

    spinlock_acquire(&irq_handlers_lock);
    list_node_append(&irq_handlers, &new_handler->list_node);
    spinlock_release(&irq_handlers_lock);
}
