// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/tasks/task_types.h>

__BEGIN_DECLS

void tasks_init();

void scheduler_init();

/**
 * @brief Unblock the scheduler, so that APs can start scheduling.
 *
 */
void unblock_scheduler(void);

/**
 * @brief Enter the scheduler and switch to the next thread.
 *
 */
noreturn void enter_scheduler(void);

/**
 * @brief Add a thread to the scheduler, so that it can be scheduled.
 *
 * @param thread
 */
void scheduler_add_thread(thread_t *thread);

/**
 * @brief Remove a thread from the scheduler.
 *
 * @param thread
 */
void scheduler_remove_thread(thread_t *thread);

/**
 * @brief Wake a thread.
 *
 * @param thread
 */
void scheduler_wake_thread(thread_t *thread);

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

__nodiscard bool reschedule_for_waitlist(waitlist_t *waitlist);

__END_DECLS
