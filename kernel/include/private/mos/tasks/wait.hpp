// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/allocator.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/mos_global.h>

/**
 * @brief The entry in the waiters list of a process, or a thread
 */
struct waitable_list_entry_t : mos::NamedType<"WaitlistEntry">
{
    as_linked_list;
    tid_t waiter;
};

struct waitlist_t : mos::NamedType<"Waitlist">
{
    bool closed = false;             // if true, then the process is closed and should not be waited on
    spinlock_t lock = SPINLOCK_INIT; // protects the waiters list
    list_head list;                  // list of threads waiting
    waitlist_t();
};

void waitlist_init(waitlist_t *list);
__nodiscard bool waitlist_append(waitlist_t *list);
size_t waitlist_wake(waitlist_t *list, size_t max_wakeups);
void waitlist_close(waitlist_t *list);
void waitlist_remove_me(waitlist_t *waitlist);

#define waitlist_wake_one(list) waitlist_wake(list, 1)
#define waitlist_wake_all(list) waitlist_wake(list, SIZE_MAX)
