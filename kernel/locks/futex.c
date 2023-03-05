// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/locks/futex.h"

#include "lib/stdlib.h"
#include "lib/structures/list.h"
#include "lib/sync/spinlock.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging/paging.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/wait.h"
#include "mos/types.h"

typedef uintptr_t futex_key_t;

typedef struct
{
    as_linked_list;
    futex_key_t key;
    size_t num_waiters;
    thread_t **waiters;
    spinlock_t lock;
} futex_private_t;

static list_node_t futex_list_head = LIST_HEAD_INIT(futex_list_head);
static spinlock_t futex_list_lock = SPINLOCK_INIT;

static futex_key_t futex_get_key(const futex_word_t *futex)
{
    const uintptr_t vaddr = (uintptr_t) futex;
    return mm_get_block_info(current_process->pagetable, vaddr, 1).paddr + vaddr % MOS_PAGE_SIZE;
}

bool futex_wait(futex_word_t *futex, futex_word_t expected)
{
    const futex_word_t current_value = __atomic_load_n(futex, __ATOMIC_SEQ_CST);

    if (current_value != expected)
    {
        //
        // The purpose of the comparison with the expected value is to prevent lost wake-ups.
        //
        // if another thread changed the futex word value after the calling thread decided to block based on the prior value
        // and, if that thread executed a FUTEX_WAKE operation (or similar wake-up) after the value change before this FUTEX_WAIT operation
        // then, with this check, the calling thread will observe the value change and will not start to sleep.
        //
        //    | thread A           | thread B           |
        //    |--------------------|--------------------|
        //    | Check futex value  |                    |
        //    | decide to block    |                    |
        //    |                    | Change futex value |
        //    |                    | Execute FUTEX_WAKE |
        //    | system call        |                    |
        //    |--------------------|--------------------|
        //    | this check fails   |                    | <--- if this check was not here, thread A would block, losing a wake-up
        //    |--------------------|--------------------|
        //    | unblocked          |                    |
        //    |--------------------|--------------------|
        //
        return false;
    }

    // firstly find the futex in the list
    // if it's not there, create a new one add the current thread to the waiters list
    // then reschedule
    const futex_key_t key = futex_get_key(futex);

    futex_private_t *fu = NULL;

    spinlock_acquire(&futex_list_lock);
    list_foreach(futex_private_t, f, futex_list_head)
    {
        if (f->key == key)
        {
            fu = f;
            spinlock_acquire(&fu->lock);
            break;
        }
    }

    if (!fu)
    {
        fu = (futex_private_t *) kzalloc(sizeof(futex_private_t));
        fu->key = key;
        spinlock_acquire(&fu->lock);
        list_node_append(&futex_list_head, list_node(fu));
    }
    spinlock_release(&futex_list_lock);

    fu->num_waiters++;
    fu->waiters = krealloc(fu->waiters, fu->num_waiters * sizeof(thread_t *));
    fu->waiters[fu->num_waiters - 1] = current_thread;
    spinlock_release(&fu->lock);

    spinlock_acquire(&current_thread->state_lock);
    current_thread->state = THREAD_STATE_BLOCKED;
    spinlock_release(&current_thread->state_lock);

    mos_debug(futex, "tid %ld waiting on lock key=" PTR_FMT, current_thread->tid, key);

    reschedule();

    mos_debug(futex, "tid %ld woke up", current_thread->tid);
    return true;
}

bool futex_wake(futex_word_t *futex, size_t num_to_wake)
{
    const futex_key_t key = futex_get_key(futex);
    if (unlikely(num_to_wake == 0))
    {
        mos_debug(futex, "tid %ld tried to release a key=" PTR_FMT " but num_to_wake was 0", current_thread->tid, key);
        return false;
    }

    futex_private_t *fu = NULL;

    spinlock_acquire(&futex_list_lock);
    list_foreach(futex_private_t, f, futex_list_head)
    {
        if (f->key == key)
        {
            fu = f;
            spinlock_acquire(&fu->lock);
            break;
        }
    }
    spinlock_release(&futex_list_lock);

    if (!fu)
    {
        mos_debug(futex, "tid %ld tried to release a lock key=" PTR_FMT " but it was already unlocked", current_thread->tid, key);
        return true;
    }

    const u32 num_to_wake_actual = MIN(num_to_wake, fu->num_waiters);

    // wake up the threads
    mos_debug(futex, "tid %ld releasing a lock key=" PTR_FMT " and waking up %d threads", current_thread->tid, key, num_to_wake_actual);
    for (u32 i = 0; i < num_to_wake_actual; i++)
    {
        thread_t *t = fu->waiters[i];
        spinlock_acquire(&t->state_lock);
        MOS_ASSERT_X(t->state == THREAD_STATE_BLOCKED, "thread %ld was not blocked", t->tid);
        t->state = THREAD_STATE_READY;
        spinlock_release(&t->state_lock);
    }

    if (num_to_wake_actual == fu->num_waiters)
    {
        // do cleanup
        list_node_remove(list_node(fu));
        kfree(fu->waiters);
        kfree(fu);
    }
    else
    {
        thread_t **new_waiters = krealloc(fu->waiters, (fu->num_waiters - num_to_wake) * sizeof(thread_t *));

        // copy the remaining waiters
        for (u32 i = 0; i < fu->num_waiters - num_to_wake; i++)
            new_waiters[i] = fu->waiters[i + num_to_wake];

        kfree(fu->waiters);
        fu->waiters = new_waiters;
        fu->num_waiters -= num_to_wake;
    }

    spinlock_release(&fu->lock);

    return true;
}
