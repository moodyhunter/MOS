// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/lib/sync/spinlock.h"
#include "mos/mm/mm.h"

#include <errno.h>
#include <limits.h>
#include <mos/lib/structures/hashmap.h>
#include <mos/lib/structures/hashmap_common.h>
#include <mos/lib/structures/list.h>
#include <mos/mm/cow.h>
#include <mos/mm/paging/paging.h>
#include <mos/platform/platform.h>
#include <mos/syslog/printk.h>
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

    hashmap_remove(&thread_table, thread->tid);

    pr_dinfo2(thread, "destroying thread %pt", (void *) thread);
    MOS_ASSERT_X(spinlock_is_locked(&thread->state_lock), "thread state lock must be held");
    MOS_ASSERT_X(thread->state == THREAD_STATE_DEAD, "thread must be dead for destroy");

    platform_context_cleanup(thread);

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

thread_t *thread_new(process_t *owner, thread_mode tmode, const char *name, size_t stack_size, void *explicit_stack_top)
{
    thread_t *t = thread_allocate(owner, tmode);

    t->name = strdup(name);

    pr_dinfo2(thread, "creating new thread %pt, owner=%pp", (void *) t, (void *) owner);

    // Kernel stack
    const ptr_t kstack_blk = phyframe_va(mm_get_free_pages(MOS_STACK_PAGES_KERNEL));
    stack_init(&t->k_stack, (void *) kstack_blk, MOS_STACK_PAGES_KERNEL * MOS_PAGE_SIZE);

    if (tmode != THREAD_MODE_USER)
    {
        stack_init(&t->u_stack, NULL, 0); // kernel thread has no user stack
        return t;
    }

    // User stack
    const size_t user_stack_size = stack_size ? stack_size : MOS_STACK_PAGES_USER * MOS_PAGE_SIZE;
    if (!explicit_stack_top)
    {
        vmap_t *stack_vmap = cow_allocate_zeroed_pages(owner->mm, user_stack_size / MOS_PAGE_SIZE, MOS_ADDR_USER_STACK, VALLOC_DEFAULT, VM_USER_RW);
        stack_init(&t->u_stack, (void *) stack_vmap->vaddr, user_stack_size);
        vmap_finalise_init(stack_vmap, VMAP_STACK, VMAP_TYPE_PRIVATE);
        return t;
    }

    // check if the stack is valid
    mm_lock_ctx_pair(owner->mm, NULL);
    vmap_t *stack_vmap = vmap_obtain(owner->mm, (ptr_t) explicit_stack_top, NULL);
    if (!stack_vmap)
    {
        pr_warn("invalid stack pointer %pt", explicit_stack_top);
        goto done_efault;
    }

    // check if the stack vmap is valid
    if (stack_vmap->content == VMAP_STACK) // has been claimed by another thread?
    {
        pr_warn("stack %pt has been claimed by another thread", explicit_stack_top);
        goto done_efault;
    }

    // check if the stack is large enough
    if (stack_vmap->npages < user_stack_size / MOS_PAGE_SIZE)
    {
        pr_warn("stack %pt is too small (size=%zu, required=%zu)", explicit_stack_top, stack_vmap->npages * MOS_PAGE_SIZE, user_stack_size);
        goto done_efault;
    }

    // check if the stack is writable
    if (!(stack_vmap->vmflags & VM_USER_RW))
    {
        pr_warn("stack %pt is not writable", explicit_stack_top);
        goto done_efault;
    }

    const ptr_t stack_bottom = ALIGN_UP_TO_PAGE((ptr_t) explicit_stack_top) - user_stack_size;
    vmap_t *second = vmap_split(stack_vmap, (stack_bottom - stack_vmap->vaddr) / MOS_PAGE_SIZE);
    spinlock_release(&stack_vmap->lock);
    stack_vmap = second;

    stack_vmap->content = VMAP_STACK;
    stack_vmap->type = VMAP_TYPE_PRIVATE;
    spinlock_release(&stack_vmap->lock);
    mm_unlock_ctx_pair(owner->mm, NULL);
    stack_init(&t->u_stack, (void *) stack_bottom, user_stack_size);
    t->u_stack.head = (ptr_t) explicit_stack_top;
    return t;

done_efault:
    spinlock_release(&stack_vmap->lock);
    mm_unlock_ctx_pair(owner->mm, NULL);
    spinlock_acquire(&t->state_lock);
    thread_destroy(t);
    return ERR_PTR(-EFAULT); // invalid stack pointer
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

void thread_exit(thread_t *t)
{
    MOS_ASSERT_X(thread_is_valid(t), "thread_handle_exit() called on invalid thread");
    spinlock_acquire(&t->state_lock);
    thread_exit_locked(t);
}

[[noreturn]] void thread_exit_locked(thread_t *t)
{
    MOS_ASSERT_X(thread_is_valid(t), "thread_exit_locked() called on invalid thread");

    pr_dinfo(thread, "thread %pt is exiting", (void *) t);

    MOS_ASSERT_X(spinlock_is_locked(&t->state_lock), "thread state lock must be held");

    t->state = THREAD_STATE_DEAD;

    waitlist_close(&t->waiters);
    waitlist_wake(&t->waiters, INT_MAX);

    while (true)
        reschedule();
    MOS_UNREACHABLE();
}
