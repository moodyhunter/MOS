// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/misc/power.hpp"

#include "mos/mm/slab.hpp"
#include "mos/mm/slab_autoinit.hpp"
#include "mos/platform/platform.hpp"
#include "mos/syslog/printk.hpp"

#include <mos/lib/structures/list.hpp>
#include <mos_stdlib.hpp>

typedef struct
{
    as_linked_list;
    power_callback_t callback;
    void *data;
} power_callback_entry_t;

static list_head pm_notifiers = LIST_HEAD_INIT(pm_notifiers);
static slab_t *power_callback_cache = NULL;
SLAB_AUTOINIT("power_callback", power_callback_cache, power_callback_entry_t);

void power_register_shutdown_callback(power_callback_t callback, void *data)
{
    power_callback_entry_t *entry = (power_callback_entry_t *) kmalloc(power_callback_cache);
    linked_list_init(list_node(entry));
    entry->callback = callback;
    entry->data = data;
    list_node_append(&pm_notifiers, list_node(entry));
}

[[noreturn]] void power_shutdown(void)
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
