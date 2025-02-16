// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/platform/platform_defs.hpp"

#include <mos/lib/structures/list.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/locks/futex.hpp>
#include <mos/mm/paging/paging.hpp>
#include <mos/platform/platform.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/schedule.hpp>
#include <mos/tasks/wait.hpp>
#include <mos/type_utils.hpp>
#include <mos/types.hpp>
#include <mos_stdlib.hpp>

typedef ptr_t futex_key_t;

struct futex_private_t : mos::NamedType<"Futex.Private">
{
    as_linked_list;
    futex_key_t key;
    waitlist_t waiters;
};

static list_head futex_list_head;
static spinlock_t futex_list_lock;

static futex_key_t futex_get_key(const futex_word_t *futex)
{
    const ptr_t vaddr = (ptr_t) futex;
    if (vaddr >= MOS_KERNEL_START_VADDR)
        return vaddr;
    return mm_get_phys_addr(current_process->mm, vaddr);
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
            break;
        }
    }

    if (!fu)
    {
        fu = mos::create<futex_private_t>();
        fu->key = key;
        waitlist_init(&fu->waiters);
        list_node_append(&futex_list_head, list_node(fu));
    }
    spinlock_release(&futex_list_lock);

    pr_dinfo2(futex, "tid %pt waiting on lock key=" PTR_FMT, (void *) current_thread, key);

    bool ok = reschedule_for_waitlist(&fu->waiters);
    MOS_ASSERT(ok);

    pr_dinfo2(futex, "tid %pt woke up", (void *) current_thread);
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
            break;
        }
    }
    spinlock_release(&futex_list_lock);

    if (!fu)
    {
        // no threads are waiting on this futex, or it's the first time it's being used by only one thread
        return true;
    }

    pr_dinfo2(futex, "waking up %zd threads on lock key=" PTR_FMT, num_to_wake, key);
    const size_t real_wakeups = waitlist_wake(&fu->waiters, num_to_wake);
    pr_dinfo2(futex, "actually woke up %zd threads", real_wakeups);

    return true;
}
