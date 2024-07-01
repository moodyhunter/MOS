// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/tasks/task_types.h>

__BEGIN_DECLS

void tasks_init();
void unblock_scheduler(void);
noreturn void scheduler(void);

void reschedule_for_wait_condition(wait_condition_t *wait_condition);
__nodiscard bool reschedule_for_waitlist(waitlist_t *waitlist);

/**
 * @brief reschedule.
 *
 */
void reschedule(void);

/**
 * @brief Mark the current task as blocked and reschedule.
 *
 */
void blocked_reschedule(void);

__END_DECLS
