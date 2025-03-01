// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/assert.hpp"
#include "mos/lib/sync/spinlock.hpp"
#include "mos/mm/mm.hpp"

#include <errno.h>
#include <limits.h>
#include <mos/lib/structures/hashmap.hpp>
#include <mos/lib/structures/hashmap_common.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/mm/cow.hpp>
#include <mos/mm/paging/paging.hpp>
#include <mos/platform/platform.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/process.hpp>
#include <mos/tasks/schedule.hpp>
#include <mos/tasks/task_types.hpp>
#include <mos/tasks/thread.hpp>
#include <mos/tasks/wait.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

mos::HashMap<tid_t, Thread *> thread_table; // tid_t -> Thread

static tid_t new_thread_id(void)
{
    static tid_t next = 1;
    return (tid_t) { next++ };
}

Thread::~Thread()
{
    pr_emerg("thread %p destroyed", this);
}

Thread *thread_allocate(Process *owner, thread_mode tflags)
{
    const auto t = mos::create<Thread>();
    t->magic = THREAD_MAGIC_THRD;
    t->tid = new_thread_id();
    t->owner = owner;
    t->state = THREAD_STATE_CREATED;
    t->mode = tflags;
    waitlist_init(&t->waiters);
    linked_list_init(&t->signal_info.pending);
    linked_list_init(list_node(t));
    owner->thread_list.push_back(t);
    return t;
}

void thread_destroy(Thread *thread)
{
    MOS_ASSERT_X(thread != current_thread, "you cannot just destroy yourself");
    if (!Thread::IsValid(thread))
        return;

    thread_table.remove(thread->tid);

    pr_dinfo2(thread, "destroying thread %pt", thread);
    MOS_ASSERT_X(spinlock_is_locked(&thread->state_lock), "thread state lock must be held");
    MOS_ASSERT_X(thread->state == THREAD_STATE_DEAD, "thread must be dead for destroy");

    platform_context_cleanup(thread);

    if (thread->mode == THREAD_MODE_USER)
    {
        const auto owner = thread->owner;
        SpinLocker lock(&owner->mm->mm_lock);
        vmap_t *const stack = vmap_obtain(owner->mm, (ptr_t) thread->u_stack.top - 1);
        vmap_destroy(stack);
    }

    mm_free_pages(va_phyframe((ptr_t) thread->k_stack.top) - MOS_STACK_PAGES_KERNEL, MOS_STACK_PAGES_KERNEL);
}

PtrResult<Thread> thread_new(Process *owner, thread_mode tmode, mos::string_view name, size_t stack_size, void *explicit_stack_top)
{
    const auto t = thread_allocate(owner, tmode);

    t->name = name;

    pr_dinfo2(thread, "creating new thread %pt, owner=%pp", t, owner);

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
        auto stack_vmap = cow_allocate_zeroed_pages(owner->mm, user_stack_size / MOS_PAGE_SIZE, MOS_ADDR_USER_STACK, VM_USER_RW);
        if (stack_vmap.isErr())
        {
            pr_emerg("failed to allocate stack for new thread");
            thread_destroy(t);
            return stack_vmap.getErr();
        }

        stack_init(&t->u_stack, (void *) stack_vmap->vaddr, user_stack_size);
        vmap_finalise_init(stack_vmap.get(), VMAP_STACK, VMAP_TYPE_PRIVATE);
        return t;
    }

    // check if the stack is valid
    mm_lock_context_pair(owner->mm);
    vmap_t *stack_vmap = vmap_obtain(owner->mm, (ptr_t) explicit_stack_top);
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

    {
        const ptr_t stack_bottom = ALIGN_UP_TO_PAGE((ptr_t) explicit_stack_top) - user_stack_size;
        vmap_t *second = vmap_split(stack_vmap, (stack_bottom - stack_vmap->vaddr) / MOS_PAGE_SIZE);
        spinlock_release(&stack_vmap->lock);
        stack_vmap = second;

        stack_vmap->content = VMAP_STACK;
        stack_vmap->type = VMAP_TYPE_PRIVATE;
        spinlock_release(&stack_vmap->lock);
        mm_unlock_context_pair(owner->mm, NULL);
        stack_init(&t->u_stack, (void *) stack_bottom, user_stack_size);
        t->u_stack.head = (ptr_t) explicit_stack_top;
        return t;
    }

done_efault:
    spinlock_release(&stack_vmap->lock);
    mm_unlock_context_pair(owner->mm, NULL);
    spinlock_acquire(&t->state_lock);
    thread_destroy(t);
    return -EFAULT; // invalid stack pointer
}

Thread *thread_complete_init(Thread *thread)
{
    if (!Thread::IsValid(thread))
        return NULL;

    thread_table.insert(thread->tid, thread);
    return thread;
}

Thread *thread_get(tid_t tid)
{
    const auto ppthread = thread_table.get(tid);
    if (ppthread == nullptr)
    {
        pr_warn("thread_get(%d) from pid %d (%s) but thread does not exist", tid, current_process->pid, current_process->name.c_str());
        return NULL;
    }

    if (Thread::IsValid(*ppthread))
        return *ppthread;

    return NULL;
}

bool thread_wait_for_tid(tid_t tid)
{
    auto target = thread_get(tid);
    if (!target)
    {
        pr_warn("wait_for_tid(%d) from pid %d (%s) but thread does not exist", tid, current_process->pid, current_process->name.c_str());
        return false;
    }

    if (target->owner != current_process)
    {
        pr_warn("wait_for_tid(%d) from process %pp but thread belongs to %pp", tid, current_process, target->owner);
        return false;
    }

    bool ok = reschedule_for_waitlist(&target->waiters);
    MOS_UNUSED(ok); // true: thread is dead, false: thread is already dead at the time of calling

    return true;
}

void thread_exit(Thread *&&t)
{
    MOS_ASSERT_X(Thread::IsValid(t), "thread_handle_exit() called on invalid thread");
    spinlock_acquire(&t->state_lock);
    thread_exit_locked(std::move(t));
}

[[noreturn]] void thread_exit_locked(Thread *&&t)
{
    MOS_ASSERT_X(Thread::IsValid(t), "thread_exit_locked() called on invalid thread");

    pr_dinfo(thread, "thread %pt is exiting", t);

    MOS_ASSERT_X(spinlock_is_locked(&t->state_lock), "thread state lock must be held");

    t->state = THREAD_STATE_DEAD;

    waitlist_close(&t->waiters);
    waitlist_wake(&t->waiters, INT_MAX);

    while (true)
        reschedule();
    MOS_UNREACHABLE();
}
