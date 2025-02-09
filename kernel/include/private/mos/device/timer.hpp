// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/lib/structures/list.hpp"
#include "mos/platform/platform.hpp"

typedef struct _ktimer ktimer_t;

typedef bool (*timer_callback_t)(ktimer_t *timer, void *arg);

typedef struct _ktimer
{
    as_linked_list;
    u64 timeout;
    thread_t *thread;
    bool ticked;
    timer_callback_t callback;
    void *arg;
} ktimer_t;

void timer_tick(void);

long timer_msleep(u64 ms);
