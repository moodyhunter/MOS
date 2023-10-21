// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/lib/structures/list.h>
#include <mos/types.h>

typedef struct clocksource
{
    as_linked_list;
    const char *const name;
    u64 ticks;     // number of ticks since boot
    u64 frequency; // ticks per second
} clocksource_t;

extern list_head clocksources;
extern clocksource_t *const active_clocksource;

void clocksource_register(clocksource_t *clocksource);

void clocksource_tick(clocksource_t *clocksource); // called by the timer interrupt handler
