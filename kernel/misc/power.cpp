// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/misc/power.hpp"

#include "mos/platform/platform.hpp"
#include "mos/syslog/printk.hpp"

#include <mos/allocator.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos_stdlib.hpp>

struct power_callback_entry_t : mos::NamedType<"PowerCallback">
{
    as_linked_list;
    power_callback_t callback;
    void *data;
};

static list_head pm_notifiers;

void power_register_shutdown_callback(power_callback_t callback, void *data)
{
    power_callback_entry_t *entry = mos::create<power_callback_entry_t>();
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
        delete e;
    }

    pr_info("Bye!");
    platform_shutdown();
}
