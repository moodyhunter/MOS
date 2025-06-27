// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/schedule.hpp"

#include <mos/lib/structures/list.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/platform/platform.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/task_types.hpp>
#include <mos/tasks/thread.hpp>
#include <mos/tasks/wait.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

bool waitlist_append(waitlist_t *list)
{
    spinlock_acquire(&list->lock);
    if (list->closed)
    {
        spinlock_release(&list->lock);
        return false;
    }

    list->waiters.push_back(current_thread->tid);
    spinlock_release(&list->lock);
    return true;
}

size_t waitlist_wake(waitlist_t *list, size_t max_wakeups)
{
    spinlock_acquire(&list->lock);

    if (list->waiters.empty())
    {
        spinlock_release(&list->lock);
        return 0;
    }

    size_t wakeups = 0;
    while (wakeups < max_wakeups && !list->waiters.empty())
    {
        const auto last = list->waiters.back();
        list->waiters.pop_back();

        Thread *thread = thread_get(last);
        if (thread) // if the thread is still there
        {
            if (thread->state == THREAD_STATE_BLOCKED)
                scheduler_wake_thread(thread);
        }
        wakeups++;
    }

    spinlock_release(&list->lock);

    return wakeups;
}

void waitlist_close(waitlist_t *list)
{
    spinlock_acquire(&list->lock);
    if (list->closed)
        pr_warn("waitlist already closed");

    list->closed = true;
    spinlock_release(&list->lock);
}

void waitlist_remove_me(waitlist_t *waitlist)
{
    spinlock_acquire(&waitlist->lock);
    for (auto it = waitlist->waiters.begin(); it != waitlist->waiters.end();)
    {
        tid_t entry = *it;
        if (entry == current_thread->tid)
        {
            it = waitlist->waiters.erase(it);
            break;
        }
        ++it;
    }
    spinlock_release(&waitlist->lock);
}
