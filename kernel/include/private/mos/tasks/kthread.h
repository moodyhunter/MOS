// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/platform/platform.h>

void kthread_init(void);

/**
 * @brief Create a kernel-mode thread.
 *
 * @param entry The entry point of the thread
 * @param arg The argument to pass to the thread
 * @param name The name of the thread
 * @return thread_t* The created thread
 */
thread_t *kthread_create(thread_entry_t entry, void *arg, const char *name);

/**
 * @brief Create a kernel thread, but do not add it to the scheduler.
 *
 * @param entry The entry point of the thread
 * @param arg The argument to pass to the thread
 * @param name The name of the thread
 * @return thread_t* The created thread
 */
__nodiscard thread_t *kthread_create_no_sched(thread_entry_t entry, void *arg, const char *name);
