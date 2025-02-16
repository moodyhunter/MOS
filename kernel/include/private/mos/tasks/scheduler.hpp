// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.hpp"

typedef struct _scheduler_ops scheduler_ops_t;
typedef struct _scheduler scheduler_t;

typedef struct _scheduler_ops
{
    void (*init)(scheduler_t *instance); ///< Initialize the scheduler

    /**
     * @brief Select the next thread to run, thread state lock should be locked
     *
     */
    Thread *(*select_next)(scheduler_t *instance);
    void (*add_thread)(scheduler_t *instance, Thread *thread);    ///< Add a thread to the scheduler
    void (*remove_thread)(scheduler_t *instance, Thread *thread); ///< Remove a thread from the scheduler
} scheduler_ops_t;

typedef struct _scheduler
{
    const scheduler_ops_t *ops;
} scheduler_t;

typedef struct _scheduler_info
{
    const char *const name;
    scheduler_t *const scheduler;
} scheduler_info_t;

#define MOS_SCHEDULER(_name, _i) MOS_PUT_IN_SECTION(".mos.schedulers", scheduler_info_t, MOS_CONCAT(_name, __COUNTER__), { .name = #_name, .scheduler = &_i })
