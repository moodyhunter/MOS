// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/device/clocksource.hpp"

#include "mos/device/timer.hpp"

list_head clocksources = LIST_HEAD_INIT(clocksources);
clocksource_t *active_clocksource;

void clocksource_register(clocksource_t *clocksource)
{
    clocksource->ticks = 0;
    list_node_append(&clocksources, list_node(clocksource));
    active_clocksource = clocksource;
}

void clocksource_tick(clocksource_t *clocksource)
{
    clocksource->ticks++;
    timer_tick();
}
