// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/lib/structures/list.hpp>
#include <mos/types.hpp>

typedef struct clocksource
{
    as_linked_list;
    const char *const name;
    u64 ticks;     // number of ticks since boot
    u64 frequency; // ticks per second
} clocksource_t;

extern list_head clocksources;
extern clocksource_t *active_clocksource;
#define active_clocksource_ticks() READ_ONCE(active_clocksource->ticks)

void clocksource_register(clocksource_t *clocksource);

void clocksource_tick(clocksource_t *clocksource); // called by the timer interrupt handler
