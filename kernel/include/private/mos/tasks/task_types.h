// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/stack.h"
#include "lib/sync/spinlock.h"
#include "mos/filesystem/fs_types.h"
#include "mos/filesystem/vfs.h"
#include "mos/io/io.h"
#include "mos/kconfig.h"
#include "mos/mm/mm_types.h"
#include "mos/platform/platform.h"
#include "mos/types.h"

typedef struct _terminal terminal_t;

typedef enum
{
    THREAD_MODE_KERNEL,
    THREAD_MODE_USER,
} thread_mode;

typedef enum
{
    VMTYPE_CODE,   // code
    VMTYPE_DATA,   // data
    VMTYPE_HEAP,   // heap
    VMTYPE_STACK,  // stack (user)
    VMTYPE_KSTACK, // stack (kernel)
    VMTYPE_FILE,   // file mapping
    VMTYPE_MMAP,   // mmap mapping
} vmap_content_t;

typedef enum
{
    VMAP_FORK_NA = 0,                 // not applicable
    VMAP_FORK_PRIVATE = MMAP_PRIVATE, // there will be distinct copies of the memory region in the child process
    VMAP_FORK_SHARED = MMAP_SHARED,   // the memory region will be shared between the parent and child processes
} vmap_fork_mode_t;

typedef struct
{
    // cow:     If the process attempts to write to a page that is marked as copy-on-write,
    //          the kernel will copy the page and mark it as read/write. This is done
    //          in the page fault handler.
    bool cow : 1;

    // This flag only applies to file and anonymous mappings
    vmap_fork_mode_t fork_mode;
} vmap_flags_t;

typedef struct
{
    vmap_content_t content;
    vmblock_t blk;

    // if any of the vmap_flags_t is set, then the flags in vm contains 'original' flags
    // of this block. Which means if there're no VM_WRITE flag, then the block
    // should not be writable.
    vmap_flags_t flags;
    spinlock_t lock;
} vmap_t;

typedef struct _wait_condition wait_condition_t;
typedef struct _thread thread_t;
typedef struct _process process_t;

typedef struct _wait_condition
{
    void *arg;
    bool (*verify)(wait_condition_t *condition); // return true if condition is met
    void (*cleanup)(wait_condition_t *condition);
} wait_condition_t;

/**
 * @brief The entry in the waiters list of a process, or a thread
 */
typedef struct
{
    as_linked_list;
    tid_t waiter;
} waitable_list_entry_t;

typedef struct
{
    bool closed;     // if true, then the process is closed and should not be waited on
    spinlock_t lock; // protects the waiters list
    list_head list;  // list of threads waiting
} waitlist_t;

typedef struct _process
{
    u32 magic;
    const char *name;
    pid_t pid;
    process_t *parent;
    terminal_t *terminal; // terminal this process's stdin/stdout/stderr are connected to
    paging_handle_t pagetable;

    argv_t argv;

    io_t *files[MOS_PROCESS_MAX_OPEN_FILES];

    ssize_t threads_count;
    thread_t *threads[MOS_PROCESS_MAX_THREADS];

    size_t mmaps_count;
    vmap_t *mmaps;

    dentry_t *working_directory;

    // platform per-process flags
    void *platform_options;
    waitlist_t waiters; // list of threads waiting for this process to exit
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
    waitlist_t waiters; // list of threads waiting for this thread to exit
} thread_t;
