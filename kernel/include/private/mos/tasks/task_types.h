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
    VMTYPE_CODE,   // code
    VMTYPE_DATA,   // data
    VMTYPE_ZERO,   // zeroed pages (typically bss)
    VMTYPE_HEAP,   // heap
    VMTYPE_STACK,  // stack (user)
    VMTYPE_KSTACK, // stack (kernel)
    VMTYPE_SHARED, // shared memory (e.g., IPC)
    VMTYPE_FILE,   // file mapping
    VMTYPE_MMAP,   // mmap mapping
} vmblock_content_t;

typedef enum
{
    // bit 0 is reserved

    // bit 1: fork behavior
    VMBLOCK_FORK_PRIVATE = 0 << 1, // child has its own copy of the page
    VMBLOCK_FORK_SHARED = 1 << 1,  // child shares the same physical pages with parent

    // bit 2, 3: CoW enabled, CoW behavior
    VMBLOCK_COW_NONE = 0 << 2,                                 // no COW
    VMBLOCK_COW_ENABLED = 1 << 2,                              // COW enabled
    VMBLOCK_COW_COPY_ON_WRITE = VMBLOCK_COW_ENABLED | 0 << 3,  // copy-on-write
    VMBLOCK_COW_ZERO_ON_DEMAND = VMBLOCK_COW_ENABLED | 1 << 3, // zero-on-demand

    VMBLOCK_DEFAULT = VMBLOCK_FORK_PRIVATE | VMBLOCK_COW_COPY_ON_WRITE,
} vmblock_flags_t;

typedef struct
{
    vmblock_content_t content;
    vmblock_t blk;

    // if any of the vmblock_cow_t is set, then the flags in vm contains 'original' flags
    // of this block. Which means if there're no VM_WRITE flag, then the block
    // should not be writable.
    vmblock_flags_t flags;

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
