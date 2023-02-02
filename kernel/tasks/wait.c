// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/wait.h"

#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

static bool thread_is_ready(wait_condition_t *condition)
{
    thread_t *thread = condition->arg;
    return thread->state == THREAD_STATE_DEAD;
}

wait_condition_t *wc_wait_for_thread(thread_t *target)
{
    return wc_wait_for(target, thread_is_ready, NULL);
}

wait_condition_t *wc_wait_for(void *arg, wait_condition_verifier_t verify, wait_condition_cleanup_t cleanup)
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
