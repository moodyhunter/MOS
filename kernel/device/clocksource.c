// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/clocksource.h"

#include "mos/platform/platform.h"
#include "mos/tasks/schedule.h"

#define RESCHEDULE_INTERVAL_MS 10
list_head clocksources = LIST_HEAD_INIT(clocksources);

void clocksource_register(clocksource_t *clocksource)
{
    clocksource->ticks = 0;
    list_node_append(&clocksources, list_node(clocksource));
    pr_info2("registered clocksource %s", clocksource->name);
}

void clocksource_tick(clocksource_t *clocksource)
{
    clocksource->ticks++;
    if (clocksource->ticks % (clocksource->frequency / 1000 * RESCHEDULE_INTERVAL_MS) == 0)
        reschedule();
}
