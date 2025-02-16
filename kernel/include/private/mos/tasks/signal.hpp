// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/platform/platform.hpp"
#include "mos/tasks/task_types.hpp"

#include <mos/allocator.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/tasks/signal_types.h>

#define ERESTARTSYS 512

/**
 * @defgroup kernel_tasks_signal kernel.tasks.signal
 * @ingroup tasks
 * @brief Signal handling.
 * @{
 */

/**
 * @brief A pending signal.
 *
 */
struct sigpending_t : mos::NamedType<"SigPending">
{
    as_linked_list;
    signal_t signal;
};

/**
 * @brief Send a signal to a thread.
 *
 * @param target
 * @param signal
 */
long signal_send_to_thread(Thread *target, signal_t signal);

/**
 * @brief Send a signal to a process, an arbitrary thread will be chosen to receive the signal.
 *
 * @param target
 * @param signal
 */
long signal_send_to_process(Process *target, signal_t signal);

/**
 * @brief Prepare to exit to userspace.
 *
 * @param regs The registers of the thread.
 *
 */
void signal_exit_to_user_prepare(platform_regs_t *regs);

/**
 * @brief Prepare to exit to userspace after a syscall.
 *
 * @param regs The registers of the thread.
 * @param syscall_nr The syscall number, used in case the syscall should be restarted.
 * @param syscall_ret The return value of the syscall, which may be -ERESTARTSYS,
 *                    in which case the syscall should be restarted.
 */
void signal_exit_to_user_prepare_syscall(platform_regs_t *regs, reg_t syscall_nr, reg_t syscall_ret);

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
 * @brief Return true if there's a pending signal.
 *
 */
bool signal_has_pending(void);

/** @} */
