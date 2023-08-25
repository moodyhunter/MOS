// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/thread.h>
#include <mos/tasks/wait.h>
#include <stdlib.h>

static slab_t *waitlist_listentry_slab = NULL;
SLAB_AUTOINIT("waitlist_entry", waitlist_listentry_slab, waitable_list_entry_t);

wait_condition_t *wc_wait_for(void *arg, wait_condition_verifier_t verify, wait_condition_cleanup_t cleanup)
{
    wait_condition_t *condition = kmalloc(sizeof(wait_condition_t));
    condition->arg = arg;
    condition->verify = verify;
    condition->cleanup = cleanup;
    return condition;
}

bool wc_condition_verify(wait_condition_t *condition)
{
    MOS_ASSERT_X(condition->verify, "wait condition has no verify function");
    return condition->verify(condition);
}

void wc_condition_cleanup(wait_condition_t *condition)
{
    if (condition->cleanup)
        condition->cleanup(condition);
    kfree(condition);
}

void waitlist_init(waitlist_t *list)
{
    linked_list_init(&list->list);
    list->lock.flag = 0;
}

bool waitlist_append_only(waitlist_t *list)
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

        if (thread)
        {
            spinlock_acquire(&thread->state_lock);
            MOS_ASSERT(thread->state == THREAD_STATE_BLOCKED);
            thread->state = THREAD_STATE_READY;
            spinlock_release(&thread->state_lock);
        }

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
