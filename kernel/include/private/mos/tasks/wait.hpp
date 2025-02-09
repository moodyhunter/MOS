// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/slab.hpp"

#include <mos/lib/structures/list.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/mos_global.h>

/**
 * @brief The entry in the waiters list of a process, or a thread
 */
typedef struct
{
    as_linked_list;
    tid_t waiter;
} waitable_list_entry_t;

typedef struct
{
    bool closed;     // if true, then the process is closed and should not be waited on
    spinlock_t lock; // protects the waiters list
    list_head list;  // list of threads waiting
} waitlist_t;

extern slab_t *waitlist_slab;

void waitlist_init(waitlist_t *list);
__nodiscard bool waitlist_append(waitlist_t *list);
size_t waitlist_wake(waitlist_t *list, size_t max_wakeups);
void waitlist_close(waitlist_t *list);
void waitlist_remove_me(waitlist_t *waitlist);

#define waitlist_wake_one(list) waitlist_wake(list, 1)
#define waitlist_wake_all(list) waitlist_wake(list, SIZE_MAX)
