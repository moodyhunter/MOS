// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.h"

#include <mos/lib/structures/hashmap.h>
#include <mos/lib/structures/hashmap_common.h>
#include <mos/lib/structures/list.h>
#include <mos/mm/cow.h>
#include <mos/mm/paging/paging.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/tasks/process.h>
#include <mos/tasks/schedule.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/thread.h>
#include <mos/tasks/wait.h>
#include <mos_stdlib.h>
#include <mos_string.h>

hashmap_t thread_table = { 0 }; // tid_t -> thread_t

static tid_t new_thread_id(void)
{
    static tid_t next = 1;
    return (tid_t){ next++ };
}

thread_t *thread_allocate(process_t *owner, thread_mode tflags)
{
    thread_t *t = kmalloc(thread_cache);
    t->magic = THREAD_MAGIC_THRD;
    t->tid = new_thread_id();
    t->owner = owner;
    t->state = THREAD_STATE_CREATED;
    t->mode = tflags;
    t->waiting = NULL;
    waitlist_init(&t->waiters);
    linked_list_init(&t->signal_info.pending);
    linked_list_init(list_node(t));
    list_node_append(&owner->threads, list_node(t));
    return t;
}

void thread_destroy(thread_t *thread)
{
    MOS_ASSERT_X(thread != current_thread, "you cannot just destroy yourself");
    if (!thread_is_valid(thread))
        return;

    pr_dinfo2(thread, "destroying thread %pt", (void *) thread);

    if (thread->name)
        kfree(thread->name);

    if (thread->mode == THREAD_MODE_USER)
    {
        process_t *const owner = thread->owner;
        spinlock_acquire(&owner->mm->mm_lock);
        vmap_t *const stack = vmap_obtain(owner->mm, (ptr_t) thread->u_stack.top - 1, NULL);
        vmap_destroy(stack);
        spinlock_release(&owner->mm->mm_lock);
    }

    mm_free_pages(va_phyframe((ptr_t) thread->k_stack.top) - MOS_STACK_PAGES_KERNEL, MOS_STACK_PAGES_KERNEL);

    kfree(thread);
}

thread_t *thread_new(process_t *owner, thread_mode tmode, const char *name)
{
    thread_t *t = thread_allocate(owner, tmode);

    t->name = strdup(name);

    pr_dinfo2(thread, "creating new thread %pt, owner=%pp", (void *) t, (void *) owner);

    // Kernel stack
    const ptr_t kstack_blk = phyframe_va(mm_get_free_pages(MOS_STACK_PAGES_KERNEL));
    stack_init(&t->k_stack, (void *) kstack_blk, MOS_STACK_PAGES_KERNEL * MOS_PAGE_SIZE);

    if (tmode == THREAD_MODE_USER)
    {
        // User stack
        vmap_t *stack_vmap = cow_allocate_zeroed_pages(owner->mm, MOS_STACK_PAGES_USER, MOS_ADDR_USER_STACK, VALLOC_DEFAULT, VM_USER_RW);
        stack_init(&t->u_stack, (void *) stack_vmap->vaddr, MOS_STACK_PAGES_USER * MOS_PAGE_SIZE);
        vmap_finalise_init(stack_vmap, VMAP_STACK, VMAP_TYPE_PRIVATE);
    }
    else
    {
        // kernel thread has no user stack
        stack_init(&t->u_stack, NULL, 0);
    }

    return t;
}

thread_t *thread_complete_init(thread_t *thread)
{
    if (!thread_is_valid(thread))
        return NULL;

    thread_t *old = hashmap_put(&thread_table, thread->tid, thread);
    MOS_ASSERT(old == NULL);
    return thread;
}

thread_t *thread_get(tid_t tid)
{
    thread_t *t = hashmap_get(&thread_table, tid);
    if (thread_is_valid(t))
        return t;

    return NULL;
}

bool thread_wait_for_tid(tid_t tid)
{
    thread_t *target = thread_get(tid);
    if (target == NULL)
    {
        pr_warn("wait_for_tid(%d) from pid %d (%s) but thread does not exist", tid, current_process->pid, current_process->name);
        return false;
    }

    if (target->owner != current_process)
    {
        pr_warn("wait_for_tid(%d) from process %pp but thread belongs to %pp", tid, (void *) current_process, (void *) target->owner);
        return false;
    }

    bool ok = reschedule_for_waitlist(&target->waiters);
    MOS_UNUSED(ok); // true: thread is dead, false: thread is already dead at the time of calling

    return true;
}

void thread_handle_exit(thread_t *t)
{
    if (!thread_is_valid(t))
        mos_panic("thread_handle_exit() called on invalid thread");

    pr_dinfo(thread, "thread %pt exited", (void *) t);

    spinlock_acquire(&t->state_lock);
    t->state = THREAD_STATE_DEAD;
    spinlock_release(&t->state_lock);

    waitlist_close(&t->waiters);
    waitlist_wake(&t->waiters, INT_MAX);

    reschedule();
    MOS_UNREACHABLE();
}
