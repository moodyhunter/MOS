// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mutex/mutex.h"

#include "mos/printk.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/wait.h"

void mutex_try_acquire_may_reschedule(bool *mutex)
{
    if ((uintptr_t) mutex < MOS_KERNEL_START_VADDR)
    {
        pr_emerg("mutex_try_acquire_may_reschedule: tid %d tried to acquire a lock at " PTR_FMT " which is in user space", current_thread->tid, (uintptr_t) mutex);
    }

    if (__sync_bool_compare_and_swap(mutex, MUTEX_UNLOCKED, MUTEX_LOCKED)) // if the mutex is unlocked, lock it
    {
        pr_info2("mutex_acquire: tid %d acquires a free lock at " PTR_FMT, current_thread->tid, (uintptr_t) mutex);
        return;
    }

    pr_info2("mutex_acquire: tid %d blocks on a locked lock at " PTR_FMT, current_thread->tid, (uintptr_t) mutex);

    // TODO: this uses a user pointer, which will DEFINITELY cause problems when we have multiple processes
    wait_condition_t *wc = wc_wait_for_mutex(mutex);
    reschedule_for_wait_condition(wc);
    pr_info2("mutex_acquire: tid %d unblocks and acquires a lock at " PTR_FMT, current_thread->tid, (uintptr_t) mutex);
    __atomic_store_n(mutex, MUTEX_LOCKED, __ATOMIC_SEQ_CST);
}

bool mutex_release(bool *lock)
{

    if (__atomic_load_n(lock, __ATOMIC_SEQ_CST) == MUTEX_UNLOCKED)
    {
        pr_warn("mutex_release: tid %d tried to release a lock at " PTR_FMT " but it was already unlocked", current_thread->tid, (uintptr_t) lock);
        return false;
    }

    if (__sync_bool_compare_and_swap(lock, MUTEX_LOCKED, MUTEX_UNLOCKED)) // if the mutex is locked, unlock it
    {
        pr_info2("mutex_release: tid %d releases a lock at " PTR_FMT, current_thread->tid, (uintptr_t) lock);
        return true;
    }

    pr_warn("mutex_release: tid %d tried to release a lock at " PTR_FMT " but it was already unlocked", current_thread->tid, (uintptr_t) lock);
    return false;
}
