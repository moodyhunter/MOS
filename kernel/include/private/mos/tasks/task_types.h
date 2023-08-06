// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mmstat.h"
#include "mos/mm/slab.h"

#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.h>
#include <mos/io/io.h>
#include <mos/kconfig.h>
#include <mos/lib/structures/stack.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/mm_types.h>
#include <mos/platform/platform.h>
#include <mos/tasks/signal_types.h>
#include <mos/tasks/wait.h>
#include <mos/types.h>

typedef enum
{
    THREAD_MODE_KERNEL,
    THREAD_MODE_USER,
} thread_mode;

typedef struct _thread thread_t;
typedef struct _process process_t;

typedef struct _process
{
    u32 magic;
    pid_t pid;
    const char *name;
    process_t *parent;

    argv_t argv;

    io_t *files[MOS_PROCESS_MAX_OPEN_FILES];

    ssize_t threads_count;
    thread_t *threads[MOS_PROCESS_MAX_THREADS];

    mm_context_t *mm;
    dentry_t *working_directory;

    // platform per-process flags
    void *platform_options;
    waitlist_t waiters; // list of threads waiting for this process to exit

    // signal handling
    signal_action_t signal_handlers[_SIGNAL_CATCHABLE_MAX];
} process_t;

typedef struct _thread
{
    u32 magic;
    tid_t tid;
    const char *name;
    process_t *owner;
    thread_mode mode;          // user-mode thread or kernel-mode
    spinlock_t state_lock;     // protects the thread state
    thread_state_t state;      // thread state
    downwards_stack_t u_stack; // user-mode stack
    downwards_stack_t k_stack; // kernel-mode stack
    thread_context_t *context; // platform-specific context
    wait_condition_t *waiting;
    waitlist_t waiters; // list of threads waiting for this thread to exit
} thread_t;

extern slab_t *process_cache, *thread_cache;
