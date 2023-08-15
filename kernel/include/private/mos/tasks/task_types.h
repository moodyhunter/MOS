// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mmstat.h"
#include "mos/mm/slab.h"

#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.h>
#include <mos/io/io.h>
#include <mos/kconfig.h>
#include <mos/lib/structures/list.h>
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
    list_head children;      // list of children processes
    list_node_t parent_node; // node in the parent's children list

    argv_t argv;

    io_t *files[MOS_PROCESS_MAX_OPEN_FILES];

    // ssize_t threads_count;
    // thread_t *threads[MOS_PROCESS_MAX_THREADS];
    thread_t *main_thread;
    list_head threads;

    mm_context_t *mm;
    dentry_t *working_directory;

    // platform per-process flags
    void *platform_options;
    waitlist_t waiters; // list of threads waiting for this process to exit

    // signal handling
    sigaction_t signal_handlers[SIGNAL_MAX_N];
} process_t;

typedef struct
{
    spinlock_t lock;
    list_head pending;        // list of pending signals
    bool masks[SIGNAL_MAX_N]; // signal masks, true if the signal is masked
} thread_signal_info_t;

typedef struct _thread
{
    u32 magic;
    tid_t tid;
    const char *name;
    process_t *owner;
    list_node_t owner_node;    // node in the process's thread list
    thread_mode mode;          // user-mode thread or kernel-mode
    spinlock_t state_lock;     // protects the thread state
    thread_state_t state;      // thread state
    downwards_stack_t u_stack; // user-mode stack
    downwards_stack_t k_stack; // kernel-mode stack
    void *context;             // platform-specific context
    wait_condition_t *waiting;
    waitlist_t waiters; // list of threads waiting for this thread to exit

    thread_signal_info_t signal_info;
} thread_t;

extern slab_t *process_cache, *thread_cache;
