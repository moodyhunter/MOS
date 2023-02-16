// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/stack.h"
#include "lib/sync/spinlock.h"
#include "mos/filesystem/fs_types.h"
#include "mos/filesystem/vfs.h"
#include "mos/io/io.h"
#include "mos/kconfig.h"
#include "mos/platform/platform.h"
#include "mos/types.h"

typedef struct _terminal terminal_t;

typedef enum
{
    THREAD_MODE_KERNEL = 0 << 0,
    THREAD_MODE_USER = 1 << 0,
} thread_mode;

typedef enum
{
    MMAP_DEFAULT = 0 << 0,        // default flags
    MMAP_COW = 1 << 0,            // this block is currently copy-on-write-mapped
    MMAP_PRIVATE = 1 << 1,        // this block is private, and should not be shared when forked
    MMAP_SHAREDMEM = 1 << 2,      // this block is shared, and should be shared when forked (aka, two processes can write to the same block)
    MMAP_ZERO_ON_DEMAND = 1 << 3, // this block should be zeroed on demand
} mmap_flags;

typedef struct
{
    vmblock_t vm;
    vm_type type;

    // if MMAP_COW is set, then the flags in vm contains 'original' flags
    // of this block. Which means if there're no VM_WRITE flag, then the block
    // should not be writable.
    mmap_flags map_flags;
    spinlock_t lock;
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
    terminal_t *terminal; // terminal this process's stdin/stdout/stderr are connected to
    uid_t effective_uid;  // effective user id (currently this is not used)
    paging_handle_t pagetable;

    argv_t argv;

    io_t *files[MOS_PROCESS_MAX_OPEN_FILES];

    ssize_t threads_count;
    thread_t *threads[MOS_PROCESS_MAX_THREADS];

    ssize_t mmaps_count;
    proc_vmblock_t *mmaps;

    dentry_t *working_directory;

    // platform per-process flags
    void *platform_options;
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
} thread_t;
