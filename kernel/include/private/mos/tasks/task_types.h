// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.h"
#include "mos/platform/platform.h"
#include "mos/tasks/wait.h"

#include <mos/lib/structures/list.h>
#include <mos/lib/structures/stack.h>
#include <mos/tasks/signal_types.h>

/**
 * @defgroup tasks Process and thread management
 * @{
 */

typedef enum
{
    THREAD_MODE_KERNEL,
    THREAD_MODE_USER,
} thread_mode;

typedef struct _thread thread_t;
typedef struct _process process_t;

typedef struct
{
    sigaction_t handlers[SIGNAL_MAX_N]; ///< signal handlers
    waitlist_t sigchild_waitlist;       ///< the parent is waiting for a child to exit, if not empty
} process_signal_info_t;

typedef struct
{
    io_t *io;
    fd_flags_t flags;
} fd_type;

#define nullfd ((fd_type){ .io = NULL, .flags = 0 })

typedef struct _process
{
    u32 magic;
    pid_t pid;
    const char *name;
    process_t *parent;
    list_head children; ///< list of children processes
    as_linked_list;     ///< node in the parent's children list

    bool exited;     ///< true if the process has exited
    u32 exit_status; ///< exit status

    fd_type files[MOS_PROCESS_MAX_OPEN_FILES];

    thread_t *main_thread;
    list_head threads;

    mm_context_t *mm;
    dentry_t *working_directory;

    platform_process_options_t platform_options; ///< platform per-process flags

    process_signal_info_t signal_info; ///< signal handling info
} process_t;

typedef struct
{
    spinlock_t lock;
    list_head pending;        ///< list of pending signals
    bool masks[SIGNAL_MAX_N]; ///< signal masks, true if the signal is masked
} thread_signal_info_t;

typedef struct _thread
{
    u32 magic;
    tid_t tid;
    const char *name;
    process_t *owner;
    as_linked_list;            ///< node in the process's thread list
    thread_mode mode;          ///< user-mode thread or kernel-mode
    spinlock_t state_lock;     ///< protects the thread state
    thread_state_t state;      ///< thread state
    downwards_stack_t u_stack; ///< user-mode stack
    downwards_stack_t k_stack; ///< kernel-mode stack

    platform_thread_options_t platform_options; ///< platform-specific thread options

    wait_condition_t *waiting; ///< wait condition this thread is waiting on
    waitlist_t waiters;        ///< list of threads waiting for this thread to exit

    thread_signal_info_t signal_info;
} thread_t;

extern slab_t *process_cache, *thread_cache;

/** @} */
