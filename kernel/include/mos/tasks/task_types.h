// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/stack.h"
#include "mos/io/io.h"
#include "mos/kconfig.h"
#include "mos/platform/platform.h"
#include "mos/types.h"

typedef struct _terminal terminal_t;

typedef enum
{
    THREAD_STATUS_READY,   // thread can be scheduled
    THREAD_STATUS_CREATED, // created or forked, but not ever started
    THREAD_STATUS_RUNNING, // thread is currently running
    THREAD_STATUS_BLOCKED, // thread is blocked by a wait condition
    THREAD_STATUS_DEAD,    // thread is dead, and will be cleaned up soon by the scheduler
} thread_status_t;

typedef enum
{
    THREAD_FLAG_KERNEL = 0 << 0,
    THREAD_FLAG_USERMODE = 1 << 0,
} thread_flags_t;

typedef enum
{
    MMAP_DEFAULT = 0 << 0,   // default flags
    MMAP_COW = 1 << 0,       // this block is currently copy-on-write-mapped
    MMAP_PRIVATE = 1 << 1,   // this block is private, and should not be shared when forked
    MMAP_SHAREDMEM = 1 << 2, // this block is shared, and should be shared when forked (aka, two processes can write to the same block)
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

typedef struct _wait_condition wait_condition_t;
typedef struct _thread thread_t;
typedef struct _process process_t;

typedef struct _wait_condition
{
    void *arg;
    bool (*verify)(wait_condition_t *condition); // return true if condition is met
    void (*cleanup)(wait_condition_t *condition);
} wait_condition_t;

typedef struct _process
{
    u32 magic;
    const char *name;
    pid_t pid;
    process_t *parent;
    terminal_t *terminal;
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
    u32 magic;
    tid_t tid;
    thread_status_t status;
    process_t *owner;
    downwards_stack_t stack;
    downwards_stack_t kernel_stack;
    platform_context_t *context;
    thread_flags_t flags;
    wait_condition_t *waiting_condition;
} thread_t;