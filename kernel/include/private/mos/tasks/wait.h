// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/tasks/task_types.h"

typedef bool (*wait_condition_verifier_t)(wait_condition_t *condition);
typedef void (*wait_condition_cleanup_t)(wait_condition_t *condition);

wait_condition_t *wc_wait_for_thread(thread_t *thread);

wait_condition_t *wc_wait_for(void *arg, wait_condition_verifier_t verify, wait_condition_cleanup_t cleanup);

bool wc_condition_verify(wait_condition_t *condition);
void wc_condition_cleanup(wait_condition_t *condition);

void waitlist_init(waitlist_t *list);
void waitlist_wait(waitlist_t *list);
size_t waitlist_wake(waitlist_t *list, size_t max_wakeups);
