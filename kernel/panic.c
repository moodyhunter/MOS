// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/panic.h"

#include "lib/containers.h"
#include "lib/stdio.h"
#include "mos/mm/kmalloc.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

typedef struct
{
    as_linked_list;
    kpanic_hook_t *hook;
} panic_hook_holder_t;

static list_node_t kpanic_hooks = LIST_HEAD_INIT(kpanic_hooks);
static kmsg_handler_t *kwarn_handler = NULL;

void kwarn_handler_set(kmsg_handler_t *handler)
{
    pr_warn("installing a new warning handler...");
    kwarn_handler = handler;
}

void kwarn_handler_remove()
{
    pr_warn("removing warning handler...");
    if (!kwarn_handler)
        mos_warn("no previous warning handler installed");
    kwarn_handler = NULL;
}

void mos_kpanic(const char *func, u32 line, const char *fmt, ...)
{
    mos_platform.interrupt_disable();

    va_list args;
    char message[PRINTK_BUFFER_SIZE] = { 0 };
    va_start(args, fmt);
    vsnprintf(message, PRINTK_BUFFER_SIZE, fmt, args);
    va_end(args);

    pr_emerg("");
    pr_fatal("!!!!!!!!!!!!!!!!!!!!!!!!");
    pr_fatal("!!!!! KERNEL PANIC !!!!!");
    pr_fatal("!!!!!!!!!!!!!!!!!!!!!!!!");
    pr_emerg("");
    pr_emerg("%s", message);
    pr_emerg("  in function: %s (line %u)", func, line);

    list_foreach(panic_hook_holder_t, holder, kpanic_hooks)
    {
        holder->hook();
    }

    while (1)
        ;
}

void mos_kwarn(const char *func, u32 line, const char *fmt, ...)
{
    va_list args;
    if (kwarn_handler)
    {
        va_start(args, fmt);
        kwarn_handler(func, line, fmt, args);
        va_end(args);
        return;
    }

    char message[PRINTK_BUFFER_SIZE] = { 0 };
    va_start(args, fmt);
    vsnprintf(message, PRINTK_BUFFER_SIZE, fmt, args);
    va_end(args);

    pr_warn("%s", message);
    pr_warn("  in function: %s (line %u)", func, line);
}

void mos_install_kpanic_hook(kpanic_hook_t *hook)
{
    panic_hook_holder_t *holder = kmalloc(sizeof(panic_hook_holder_t));
    holder->hook = hook;
    list_node_append(&kpanic_hooks, &holder->list_node);
}
