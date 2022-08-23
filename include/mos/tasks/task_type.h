// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/stack.h"
#include "mos/filesystem/file.h"
#include "mos/types.h"

typedef void (*thread_entry_t)(void *arg);

typedef struct
{
    file_open_mode flags;
    file_t *file;
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
    THREAD_FLAG_USERMODE = 1 << 0,
} thread_flags_t;

typedef struct process
{
    process_id_t pid;
    process_id_t parent_pid;
    uid_t effective_uid;
    page_dir_t *page_dir;
    file_descriptor_t *file_table[MAX_FD_COUNT];
    thread_id_t main_thread_id;
} process_t;

typedef struct
{
    // read by the IRET instruction, in reversed order
    reg_t ss;     // stack segment
    reg_t esp;    // stack pointer
    reg_t eflags; // flags
    reg_t cs;     // code segment
    reg_t eip;    // instruction pointer

    reg_t eax, ebx, ecx, edx; // base, count, data registers
    reg_t esi, edi;           // source, destination registers
    reg_t ebp;                // frame pointer
    reg_t ds, es, fs, gs;     // segment registers

    // reg_t func;
    // reg_t errcode;
} x86_context_t;

typedef struct
{
    thread_id_t thread_id;
    process_id_t owner_pid;
    thread_entry_t entry_point;
    task_status_t status;
    void *arg;
    x86_context_t context;
    downwards_stack_t stack;
    thread_flags_t flags;
} thread_t;
