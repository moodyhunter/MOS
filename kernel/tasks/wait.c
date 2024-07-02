// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/tasks/schedule.h"

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/platform/platform.h>
#include <mos/syslog/printk.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/thread.h>
#include <mos/tasks/wait.h>
#include <mos_stdlib.h>
#include <mos_string.h>

slab_t *waitlist_slab = NULL;
SLAB_AUTOINIT("waitlist", waitlist_slab, waitlist_t);

static slab_t *waitlist_listentry_slab = NULL;
SLAB_AUTOINIT("waitlist_entry", waitlist_listentry_slab, waitable_list_entry_t);

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

    waitable_list_entry_t *entry = kmalloc(waitlist_listentry_slab);
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

        thread_t *thread = thread_get(entry->waiter);
        MOS_ASSERT(thread);
        if (thread->state == THREAD_STATE_BLOCKED)
            scheduler_wake_thread(thread);
        kfree(entry);
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
            kfree(entry);
            break;
        }
    }

    spinlock_release(&waitlist->lock);
}
