// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/wait.h"

#include "mos/mm/kmalloc.h"

static bool wait_for_thread_is_ready(wait_condition_t *condition)
{
    thread_t *thread = condition->arg;
    return thread->status == THREAD_STATUS_DEAD;
}

static bool wait_for_mutex_is_ready(wait_condition_t *condition)
{
    // return true if the mutex is free (aka false)
    bool *mutex = condition->arg;
    return *mutex == MUTEX_UNLOCKED; // TODO: this is unsafe in SMP
}

wait_condition_t *wc_wait_for_thread(thread_t *target)
{
    wait_condition_t *condition = kmalloc(sizeof(wait_condition_t));
    condition->arg = target;
    condition->verify = wait_for_thread_is_ready;
    condition->cleanup = NULL;
    return condition;
}

wait_condition_t *wc_wait_for_mutex(bool *mutex)
{
    wait_condition_t *condition = kmalloc(sizeof(wait_condition_t));
    condition->arg = mutex;
    condition->verify = wait_for_mutex_is_ready;
    condition->cleanup = NULL;
    return condition;
}

void wc_condition_cleanup(wait_condition_t *condition)
{
    if (condition->cleanup)
        condition->cleanup(condition);
    kfree(condition);
}
