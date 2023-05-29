// SPDX-License-Identifier: GPL-3.0-or-later

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
#include <stdlib.h>
#include <string.h>

hashmap_t *thread_table = NULL; // tid_t -> thread_t

static tid_t new_thread_id(void)
{
    static tid_t next = 1;
    return (tid_t){ next++ };
}

thread_t *thread_allocate(process_t *owner, thread_mode tflags)
{
    thread_t *t = kzalloc(sizeof(thread_t));
    t->magic = THREAD_MAGIC_THRD;
    t->tid = new_thread_id();
    t->owner = owner;
    t->state = THREAD_STATE_CREATING;
    t->mode = tflags;
    t->waiting = NULL;
    waitlist_init(&t->waiters);

    return t;
}

thread_t *thread_new(process_t *owner, thread_mode tmode, const char *name, thread_entry_t entry, void *arg)
{
    thread_t *t = thread_allocate(owner, tmode);

    t->name = strdup(name);

    // Kernel stack
    const ptr_t kstack_hint_addr = (tmode == THREAD_MODE_KERNEL) ? MOS_ADDR_KERNEL_HEAP : MOS_ADDR_USER_STACK;
    const vmblock_t kstack_blk = mm_alloc_pages(owner->pagetable, MOS_STACK_PAGES_KERNEL, kstack_hint_addr, VALLOC_DEFAULT, VM_RW);
    stack_init(&t->k_stack, (void *) kstack_blk.vaddr, kstack_blk.npages * MOS_PAGE_SIZE);
    process_attach_mmap(owner, kstack_blk, VMTYPE_KSTACK, (vmap_flags_t){ 0 });

    if (tmode == THREAD_MODE_USER)
    {
        // User stack
        const vmblock_t ustack_blk = mm_alloc_zeroed_pages(owner->pagetable, MOS_STACK_PAGES_USER, MOS_ADDR_USER_STACK, VALLOC_DEFAULT, VM_USER_RW);
        stack_init(&t->u_stack, (void *) ustack_blk.vaddr, MOS_STACK_PAGES_USER * MOS_PAGE_SIZE);
        process_attach_mmap(owner, ustack_blk, VMTYPE_STACK, (vmap_flags_t){ .cow = true });
    }
    else
    {
        // kernel thread has no user stack
        stack_init(&t->u_stack, NULL, 0);
    }

    platform_context_setup(t, entry, arg);
    process_attach_thread(owner, t);
    hashmap_put(thread_table, t->tid, t);

    return t;
}

thread_t *thread_setup_complete(thread_t *thread)
{
    if (!thread_is_valid(thread))
        return NULL;

    MOS_ASSERT(thread->state == THREAD_STATE_CREATING);
    spinlock_acquire(&thread->state_lock);
    thread->state = THREAD_STATE_CREATED;
    spinlock_release(&thread->state_lock);
    return thread;
}

thread_t *thread_get(tid_t tid)
{
    thread_t *t = hashmap_get(thread_table, tid);
    if (thread_is_valid(t))
        return t;

    return NULL;
}

bool thread_wait_for_tid(tid_t tid)
{
    thread_t *target = thread_get(tid);
    if (target == NULL)
    {
        pr_warn("wait_for_tid(%ld) from pid %ld (%s) but thread does not exist", tid, current_process->pid, current_process->name);
        return false;
    }

    if (target->owner != current_process)
    {
        pr_warn("wait_for_tid(%ld) from pid %ld (%s) but thread belongs to pid %ld (%s)", tid, current_process->pid, current_process->name, target->owner->pid,
                target->owner->name);
        return false;
    }

    if (target->state == THREAD_STATE_DEAD)
    {
        return true; // thread is already dead, no need to wait
    }

    bool ok = reschedule_for_waitlist(&target->waiters);
    MOS_UNUSED(ok);

    return true;
}

void thread_handle_exit(thread_t *t)
{
    if (!thread_is_valid(t))
        mos_panic("thread_handle_exit() called on invalid thread");

    process_t *owner = t->owner;
    vmblock_t kstack = { 0 };
    vmblock_t ustack = { 0 };
    for (size_t i = 0; i < owner->mmaps_count; i++)
    {
        vmap_t *blk = &owner->mmaps[i];
        if (blk->content != VMTYPE_KSTACK && blk->content != VMTYPE_STACK)
            continue;

        ptr_t kstack_start = (ptr_t) t->k_stack.top - t->k_stack.capacity;
        ptr_t ustack_start = (ptr_t) t->u_stack.top - t->u_stack.capacity;

        if (blk->blk.vaddr >= kstack_start && blk->blk.vaddr < kstack_start + t->k_stack.capacity)
            kstack = blk->blk;
        else if (blk->blk.vaddr >= ustack_start && blk->blk.vaddr < ustack_start + t->u_stack.capacity)
            ustack = blk->blk;
    }

    // process_detach_mmap(owner, kstack); // we are using this kernel stack, so we can't free it
    // process_detach_mmap(owner, ustack);
    MOS_UNUSED(kstack);
    MOS_UNUSED(ustack);

    spinlock_acquire(&t->state_lock);
    t->state = THREAD_STATE_DEAD;
    spinlock_release(&t->state_lock);

    waitlist_close(&t->waiters);
    waitlist_wake(&t->waiters, INT_MAX);

    reschedule();
    MOS_UNREACHABLE();
}
