// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/stack.h"
#include "mos/io/io.h"
#include "mos/kconfig.h"
#include "mos/platform/platform.h"
#include "mos/types.h"

typedef enum
{
    THREAD_STATUS_CREATED, // created, not ever started
    THREAD_STATUS_FORKED,  // forked, not ever started
    THREAD_STATUS_READY,   // thread can be scheduled
    THREAD_STATUS_RUNNING, // thread is currently running
    THREAD_STATUS_BLOCKED, // ?
    THREAD_STATUS_DEAD,    // thread is dead, and will be cleaned up soon by the scheduler
} thread_status_t;

typedef enum
{
    THREAD_FLAG_KERNEL = 0 << 0,
    THREAD_FLAG_USERMODE = 1 << 0,
} thread_flags_t;

typedef enum
{
    MMAP_DEFAULT = 0 << 0,
    MMAP_COW = 1 << 0,
    MMAP_PRIVATE = 1 << 1,
} mmap_flags;

typedef struct
{
    vmblock_t vm;
    vm_type type;

    // if MMAP_COW is set, then the flags in vm contains 'original' flags
    // of this block. Which means if there're no VM_WRITE flag, then the block
    // should not be writable.
    mmap_flags map_flags;
} proc_vmblock_t;

typedef struct _thread thread_t;
typedef struct _process process_t;

typedef struct _process
{
    char magic[4];
    const char *name;
    pid_t pid;
    process_t *parent;
    uid_t effective_uid;
    paging_handle_t pagetable;

    ssize_t files_count;
    io_t *files[MOS_PROCESS_MAX_OPEN_FILES];

    ssize_t threads_count;
    thread_t *threads[MOS_PROCESS_MAX_THREADS];

    ssize_t mmaps_count;
    proc_vmblock_t *mmaps;
} process_t;

typedef struct _thread
{
    char magic[4];
    tid_t tid;
    thread_status_t status;
    process_t *owner;
    downwards_stack_t stack;
    downwards_stack_t kernel_stack;
    platform_context_t *context;
    thread_flags_t flags;
} thread_t;
