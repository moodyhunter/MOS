// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/stack.h"
#include "mos/io/io.h"
#include "mos/kconfig.h"
#include "mos/platform/platform.h"
#include "mos/types.h"

typedef enum
{
    THREAD_STATUS_READY,
    THREAD_STATUS_RUNNING,
    THREAD_STATUS_WAITING,
    THREAD_STATUS_DYING,
    THREAD_STATUS_DEAD,
} thread_status_t;

typedef enum
{
    THREAD_FLAG_KERNEL = 0 << 0,
    THREAD_FLAG_USERMODE = 1 << 0,
} thread_flags_t;

typedef struct _thread thread_t;

typedef struct
{
    char magic[4];
    const char *name;
    pid_t pid;
    pid_t parent_pid;
    uid_t effective_uid;
    paging_handle_t pagetable;

    ssize_t files_count;
    io_t *files[MOS_PROCESS_MAX_OPEN_FILES];

    thread_t *main_thread;

    ssize_t threads_count;
    thread_t *threads[MOS_PROCESS_MAX_THREADS];

    ssize_t mmaps_count;
    proc_vmblock_t *mmaps;
} process_t;

typedef struct _thread
{
    char magic[4];
    tid_t tid;
    process_t *owner;
    thread_status_t status;
    downwards_stack_t stack;
    thread_flags_t flags;
} thread_t;
