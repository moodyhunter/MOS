// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/wait.h"

#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

static bool thread_is_ready(wait_condition_t *condition)
{
    thread_t *thread = condition->arg;
    return thread->status == THREAD_STATUS_DEAD;
}

static bool mutex_is_ready(wait_condition_t *condition)
{
    // return true if the mutex is free (aka false)
    bool *mutex = condition->arg;
    return *mutex == MUTEX_UNLOCKED; // TODO: this is unsafe in SMP
}

wait_condition_t *wc_wait_for_thread(thread_t *target)
{
    return wc_wait_for(target, thread_is_ready, NULL);
}

wait_condition_t *wc_wait_for_mutex(bool *mutex)
{
    return wc_wait_for(mutex, mutex_is_ready, NULL);
}

wait_condition_t *wc_wait_for(void *arg, bool (*verify)(wait_condition_t *condition), void (*cleanup)(wait_condition_t *condition))
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
