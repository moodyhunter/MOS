// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/stack.h"
#include "mos/io/io.h"
#include "mos/types.h"

typedef void (*thread_entry_t)(void *arg);

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

typedef struct
{
    process_id_t id;
    process_id_t parent_pid;
    uid_t effective_uid;
    paging_handle_t pagetable;
    io_t *file_table[NOS_MAX_OPEN_FILES];
    size_t files_count;
    thread_id_t main_thread_id;
} process_t;

typedef struct _thread
{
    thread_id_t id;
    process_t *owner;
    thread_status_t status;
    downwards_stack_t stack;
    thread_flags_t flags;
} thread_t;
