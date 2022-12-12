// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/tasks/task_types.h"

enum
{
    MUTEX_LOCKED = true,
    MUTEX_UNLOCKED = false,
};

wait_condition_t *wc_wait_for_thread(thread_t *thread);
wait_condition_t *wc_wait_for_mutex(bool *mutex);

wait_condition_t *wc_wait_for(void *arg, bool (*verify)(wait_condition_t *condition), void (*cleanup)(wait_condition_t *condition));

bool wc_condition_verify(wait_condition_t *condition);
void wc_condition_cleanup(wait_condition_t *condition);
