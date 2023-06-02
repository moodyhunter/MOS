// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/misc/power.h"

#include "mos/mm/slab.h"
#include "mos/platform/platform.h"

#include <mos/lib/structures/list.h>
#include <stdlib.h>

static list_head pm_notifiers = LIST_HEAD_INIT(pm_notifiers);
static slab_t *power_callback_cache = NULL;

typedef struct
{
    as_linked_list;
    power_callback_t callback;
    void *data;
} power_callback_entry_t;

void power_init()
{
    power_callback_cache = kmemcache_create("power_callback", sizeof(power_callback_entry_t));
}

void power_register_shutdown_callback(power_callback_t callback, void *data)
{
    power_callback_entry_t *entry = kmemcache_alloc(power_callback_cache);
    linked_list_init(list_node(entry));
    entry->callback = callback;
    entry->data = data;
    list_node_append(&pm_notifiers, list_node(entry));
}

noreturn void power_shutdown(void)
{
    pr_info("system shutdown initiated");
    list_foreach(power_callback_entry_t, e, pm_notifiers)
    {
        e->callback(e->data);
        list_remove(e);
        kfree(e);
    }

    pr_info("Bye!");
    platform_shutdown();
}
