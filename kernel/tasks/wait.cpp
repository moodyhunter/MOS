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

void waitlist_init(waitlist_t *list)
{
    memzero(list, sizeof(waitlist_t));
    linked_list_init(&list->list);
}

bool waitlist_append(waitlist_t *list)
{
    spinlock_acquire(&list->lock);
    if (list->closed)
    {
        spinlock_release(&list->lock);
        return false;
    }

    waitable_list_entry_t *entry = mos::create<waitable_list_entry_t>();
    entry->waiter = current_thread->tid;
    list_node_append(&list->list, list_node(entry));
    spinlock_release(&list->lock);
    return true;
}

size_t waitlist_wake(waitlist_t *list, size_t max_wakeups)
{
    spinlock_acquire(&list->lock);

    if (list_is_empty(&list->list))
    {
        spinlock_release(&list->lock);
        return 0;
    }

    size_t wakeups = 0;
    while (wakeups < max_wakeups && !list_is_empty(&list->list))
    {
        list_node_t *node = list_node_pop(&list->list);
        waitable_list_entry_t *entry = list_entry(node, waitable_list_entry_t);

        Thread *thread = thread_get(entry->waiter);
        if (thread) // if the thread is still there
        {
            if (thread->state == THREAD_STATE_BLOCKED)
                scheduler_wake_thread(thread);
        }
        delete entry;
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

    list_foreach(waitable_list_entry_t, entry, waitlist->list)
    {
        if (entry->waiter == current_thread->tid)
        {
            list_remove(entry);
            delete entry;
            break;
        }
    }

    spinlock_release(&waitlist->lock);
}

waitlist_t::waitlist_t()
{
    waitlist_init(this);
}
