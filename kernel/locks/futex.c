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
    spinlock_t lock;
    waitlist_t waiters;
} futex_private_t;

static list_head futex_list_head = LIST_HEAD_INIT(futex_list_head);
static spinlock_t futex_list_lock = SPINLOCK_INIT;

static futex_key_t futex_get_key(const futex_word_t *futex)
{
    const uintptr_t vaddr = (uintptr_t) futex;
    return platform_mm_get_phys_addr(current_process->pagetable, vaddr);
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
        // and, if that thread executed a futex_wake (or similar wake-up) after the value change before this FUTEX_WAIT operation
        // then, with this check, the calling thread will observe the value change and will not start to sleep.
        //
        //    | thread A           | thread B           |
        //    |--------------------|--------------------|
        //    | Check futex value  |                    |
        //    | decide to block    |                    |
        //    |                    | Change futex value |
        //    |                    | Execute futex_wake |
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
        waitlist_init(&fu->waiters);
        spinlock_acquire(&fu->lock);
        list_node_append(&futex_list_head, list_node(fu));
    }
    spinlock_release(&futex_list_lock);

    MOS_ASSERT_X(waitlist_wait(&fu->waiters), "waitlist_wait failed");
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
    if (unlikely(num_to_wake == 0))
        mos_panic("insane number of threads to wake up (?): %zd", num_to_wake);

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
    spinlock_release(&futex_list_lock);

    if (!fu)
    {
        // no threads are waiting on this futex, or it's the first time it's being used by only one thread
        return true;
    }

    mos_debug(futex, "waking up %zd threads on lock key=" PTR_FMT, num_to_wake, key);
    const size_t real_wakeups = waitlist_wake(&fu->waiters, num_to_wake);
    mos_debug(futex, "actually woke up %zd threads", real_wakeups);

    spinlock_release(&fu->lock);

    return true;
}
