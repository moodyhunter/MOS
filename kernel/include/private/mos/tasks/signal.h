// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"

#include <mos/lib/structures/list.h>
#include <mos/tasks/signal_types.h>

/**
 * @defgroup kernel_tasks_signal kernel.tasks.signal
 * @ingroup kernel_tasks
 * @brief Signal handling.
 * @{
 */

/**
 * @brief A pending signal.
 *
 */
typedef struct
{
    as_linked_list;
    signal_t signal;
} sigpending_t;
extern slab_t *sigpending_slab;

/**
 * @brief Send a signal to a thread.
 *
 * @param target
 * @param signal
 */
void signal_send_to_thread(thread_t *target, signal_t signal);

/**
 * @brief Send a signal to a process, an arbitrary thread will be chosen to receive the signal.
 *
 * @param target
 * @param signal
 */
void signal_send_to_process(process_t *target, signal_t signal);

/**
 * @brief Called when a system call is about to return, to check if a signal should be handled.
 *
 */
sigpending_t *signal_get_next_pending(void);

/**
 * @brief Check if there is a pending signal and handle it.
 *
 */
void signal_check_and_handle(void);

typedef struct _sigreturn_data
{
    signal_t signal;
    bool was_masked;
} sigreturn_data_t;

/**
 * @brief Return from a signal handler.
 *
 */
void signal_on_returned(sigreturn_data_t *supplimentary_data);
/**
 * @}
 */
