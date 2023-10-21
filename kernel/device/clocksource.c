// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/clocksource.h"

#include "mos/platform/platform.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/wait.h"

#define RESCHEDULE_INTERVAL_MS 10
list_head clocksources = LIST_HEAD_INIT(clocksources);
clocksource_t *active_clocksource;

void clocksource_register(clocksource_t *clocksource)
{
    clocksource->ticks = 0;
    list_node_append(&clocksources, list_node(clocksource));
    pr_info2("registered clocksource %s", clocksource->name);
    active_clocksource = clocksource;
}

void clocksource_tick(clocksource_t *clocksource)
{
    clocksource->ticks++;
    if (clocksource->ticks % (clocksource->frequency / 1000 * RESCHEDULE_INTERVAL_MS) == 0)
        reschedule();
}

#define active_clocksource_ticks() READ_ONCE(active_clocksource->ticks)

static bool should_continue(wait_condition_t *cond)
{
    return active_clocksource_ticks() >= (u64) cond->arg;
}

void clocksource_msleep(u64 ms)
{
    const u64 offset = ms * active_clocksource->frequency / 1000;
    const u64 target_val = active_clocksource_ticks() + offset;
    reschedule_for_wait_condition(wc_wait_for((void *) target_val, should_continue, NULL));
}
