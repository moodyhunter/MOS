// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/tasks/task_types.hpp>

void scheduler_init();

char thread_state_str(thread_state_t state);

/**
 * @brief Unblock the scheduler, so that APs can start scheduling.
 *
 */
void unblock_scheduler(void);

/**
 * @brief Enter the scheduler and switch to the next thread.
 *
 */
[[noreturn]] void enter_scheduler(void);

/**
 * @brief Add a thread to the scheduler, so that it can be scheduled.
 *
 * @param thread
 */
void scheduler_add_thread(Thread *thread);

/**
 * @brief Remove a thread from the scheduler.
 *
 * @param thread
 */
void scheduler_remove_thread(Thread *thread);

/**
 * @brief Wake a thread.
 *
 * @param thread
 */
void scheduler_wake_thread(Thread *thread);

/**
 * @brief reschedule.
 * @warning The caller must have the current thread's state_lock acquired.
 */
void reschedule(void);

/**
 * @brief Mark the current task as blocked and reschedule.
 *
 */
void blocked_reschedule(void);

__nodiscard bool reschedule_for_waitlist(waitlist_t *waitlist);
