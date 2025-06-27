// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/allocator.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/list.hpp>
#include <mos/mos_global.h>

struct waitlist_t : mos::NamedType<"Waitlist">
{
    explicit waitlist_t() {};

    void reset()
    {
        spinlock_acquire(&lock);
        closed = false;
        waiters.clear();
        spinlock_release(&lock);
    }

    spinlock_t lock = SPINLOCK_INIT; // protects the waiters list
    mos::list<tid_t> waiters;        // list of threads waiting
    bool closed = false;             // if true, then the process is closed and should not be waited on
};

__nodiscard bool waitlist_append(waitlist_t *list);
size_t waitlist_wake(waitlist_t *list, size_t max_wakeups);
void waitlist_close(waitlist_t *list);
void waitlist_remove_me(waitlist_t *waitlist);

#define waitlist_wake_one(list) waitlist_wake(list, 1)
#define waitlist_wake_all(list) waitlist_wake(list, SIZE_MAX)
