// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/lib/structures/list.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/platform/platform.h"
#include "mos/tasks/scheduler.h"
#include "mos/tasks/task_types.h"

#include <mos_stdlib.h>

typedef struct
{
    scheduler_t base;
    list_head threads; ///< list of runnable threads
    spinlock_t lock;
} naive_sched_t;

typedef struct
{
    as_linked_list;
    thread_t *thread;
} naive_sched_node_t; ///< Node in the naive scheduler's list of threads

static slab_t *naivesched_node_slab = NULL;
SLAB_AUTOINIT("naivesched_node", naivesched_node_slab, naive_sched_node_t);

static void naive_sched_init(scheduler_t *instance)
{
    naive_sched_t *scheduler = container_of(instance, naive_sched_t, base);
    spinlock_init(&scheduler->lock);
    linked_list_init(&scheduler->threads);
    pr_dinfo2(naive_sched, "naive scheduler initialized");
}

static thread_t *naive_sched_select_next(scheduler_t *instance)
{
    naive_sched_t *scheduler = container_of(instance, naive_sched_t, base);

    spinlock_acquire(&scheduler->lock);
    if (list_is_empty(&scheduler->threads))
    {
        spinlock_release(&scheduler->lock);
        pr_dinfo(naive_sched, "no threads to run");
        return NULL;
    }

    naive_sched_node_t *node = list_entry(scheduler->threads.next, naive_sched_node_t);
    list_remove(node);
    spinlock_release(&scheduler->lock);

    thread_t *thread = node->thread;
    kfree(node);

    if (thread == current_thread)
        spinlock_assert_locked(&thread->state_lock);
    else
        spinlock_acquire(&thread->state_lock);

    pr_dinfo2(naive_sched, "naive scheduler selected thread %pt", (void *) thread);
    return thread;
}

static void naive_sched_add_thread(scheduler_t *instance, thread_t *thread)
{
    naive_sched_t *scheduler = container_of(instance, naive_sched_t, base);

    pr_dinfo(naive_sched, "adding thread %pt to scheduler", (void *) thread);

    naive_sched_node_t *node = kmalloc(naivesched_node_slab);
    linked_list_init(list_node(node));
    node->thread = thread;

    spinlock_acquire(&scheduler->lock);
    list_node_append(&scheduler->threads, list_node(node));
    spinlock_release(&scheduler->lock);
}

static void naive_sched_remove_thread(scheduler_t *instance, thread_t *thread)
{
    pr_dinfo2(naive_sched, "naive scheduler removed thread %pt", (void *) thread);

    naive_sched_t *scheduler = container_of(instance, naive_sched_t, base);
    spinlock_acquire(&scheduler->lock);
    list_foreach(naive_sched_node_t, node, scheduler->threads)
    {
        if (node->thread == thread)
        {
            list_remove(node);
            kfree(node);
            break;
        }
    }
    spinlock_release(&scheduler->lock);
}

static const scheduler_ops_t naive_sched_ops = {
    .init = naive_sched_init,
    .select_next = naive_sched_select_next,
    .add_thread = naive_sched_add_thread,
    .remove_thread = naive_sched_remove_thread,
};

static naive_sched_t naive_sched = {
    .base.ops = &naive_sched_ops,
};

MOS_SCHEDULER(naive, naive_sched.base);
