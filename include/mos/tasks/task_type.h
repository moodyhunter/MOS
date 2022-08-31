// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/stack.h"
#include "mos/io/io.h"
#include "mos/types.h"

#define MAX_FD_COUNT 512

typedef void (*thread_entry_t)(void *arg);

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
    io_t *file_table[MAX_FD_COUNT];
    size_t files_count;
    thread_id_t main_thread_id;
} process_t;

typedef struct
{
    // do not change the order of the following members
    reg_t stack_ptr;
    reg_t instruction_ptr;
} __packed thread_context_t;

#define as_context_t              thread_context_t __mos_common_context
#define get_context_t(ctx, type)  container_of((ctx), type, __mos_common_context)
#define get_common_context_t(ctx) (&(ctx)->__mos_common_context)

typedef struct
{
    thread_id_t id;
    process_id_t owner;
    thread_entry_t entry_point;
    task_status_t status;
    void *arg;
    thread_context_t *context;
    downwards_stack_t stack;
    thread_flags_t flags;
} thread_t;
