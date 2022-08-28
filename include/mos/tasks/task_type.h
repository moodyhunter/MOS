// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/stack.h"
#include "mos/filesystem/filesystem.h"
#include "mos/platform/platform.h"
#include "mos/types.h"

typedef struct
{
    file_open_flags flags;
    io_t *file;
} file_descriptor_t;

#define MAX_FD_COUNT 512

typedef enum
{
    TASK_STATUS_READY,
    TASK_STATUS_RUNNING,
    TASK_STATUS_WAITING,
    TASK_STATUS_DYING,
    TASK_STATUS_DEAD,
} task_status_t;

typedef enum
{
    THREAD_FLAG_KTHREAD = 1 << 0,
    THREAD_FLAG_USERMODE = 1 << 1,
} thread_flags_t;

typedef struct
{
    process_id_t id;
    process_id_t parent_pid;
    uid_t effective_uid;
    paging_handle_t pagetable;
    file_descriptor_t *file_table[MAX_FD_COUNT];
    thread_id_t main_thread_id;
} process_t;

typedef struct
{
    thread_id_t id;
    process_id_t owner;
    thread_entry_t entry_point;
    task_status_t status;
    void *arg;
    mos_thread_common_context_t *context;
    downwards_stack_t stack;
    thread_flags_t flags;
} thread_t;
