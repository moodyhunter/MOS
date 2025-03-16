// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.hpp"
#include "mos/platform/platform.hpp"
#include "mos/syslog/syslog.hpp"
#include "mos/tasks/wait.hpp"

#include <abi-bits/signal.h>
#include <mos/allocator.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/structures/stack.hpp>
#include <mos/list.hpp>
#include <mos/shared_ptr.hpp>
#include <mos/string.hpp>
#include <mos/tasks/signal_types.h>
#include <mos/type_utils.hpp>

MOS_ENUM_FLAGS(FDFlag, FDFlags);

/**
 * @defgroup tasks Process and thread management
 * @{
 */

typedef enum
{
    THREAD_MODE_KERNEL,
    THREAD_MODE_USER,
} thread_mode;

struct Thread;

typedef struct
{
    sigaction_t handlers[SIGNAL_MAX_N]; ///< signal handlers
    waitlist_t sigchild_waitlist;       ///< the parent is waiting for a child to exit, if not empty
} process_signal_info_t;

struct fd_type
{
    IO *io;
    FDFlags flags;
};

inline const fd_type nullfd{ nullptr, FD_FLAGS_NONE };

#define PROCESS_MAGIC_PROC MOS_FOURCC('P', 'R', 'O', 'C')
#define THREAD_MAGIC_THRD  MOS_FOURCC('T', 'H', 'R', 'D')

struct Process : mos::NamedType<"Process">
{
    PrivateTag;

  public:
    explicit Process(Private, Process *parent, mos::string_view name);
    ~Process();

    const u32 magic = PROCESS_MAGIC_PROC;
    pid_t pid;
    mos::string name;
    Process *parent;
    list_head children; ///< list of children processes
    as_linked_list;     ///< node in the parent's children list

    bool exited;     ///< true if the process has exited
    u32 exit_status; ///< exit status

    fd_type files[MOS_PROCESS_MAX_OPEN_FILES];

    Thread *main_thread;
    mos::list<Thread *> thread_list;

    MMContext *mm;
    dentry_t *working_directory;

    platform_process_options_t platform_options; ///< platform per-process flags

    process_signal_info_t signal_info; ///< signal handling info

  public:
    static inline bool IsValid(const Process *process)
    {
        if (const auto ptr = process)
            return ptr->magic == PROCESS_MAGIC_PROC;
        else
            return false;
    }

  public:
    static inline Process *New(Process *parent, mos::string_view name)
    {
        return mos::create<Process>(Private(), parent, name);
    }

    friend mos::SyslogStreamWriter operator<<(mos::SyslogStreamWriter stream, const Process *process)
    {
        if (!Process::IsValid(process))
            return stream << "[invalid]";

        return stream << fmt("[p{}:{}]", process->pid, process->name.value_or("<no name>"));
    }
};

typedef struct
{
    spinlock_t lock;
    list_head pending; ///< list of pending signals
    sigset_t mask;     ///< pending signals mask
} thread_signal_info_t;

struct Thread : mos::NamedType<"Thread">
{
    u32 magic;
    tid_t tid;
    mos::string name;
    Process *owner;
    as_linked_list;            ///< node in the process's thread list
    thread_mode mode;          ///< user-mode thread or kernel-mode
    spinlock_t state_lock;     ///< protects the thread state
    thread_state_t state;      ///< thread state
    downwards_stack_t u_stack; ///< user-mode stack
    downwards_stack_t k_stack; ///< kernel-mode stack

    platform_thread_options_t platform_options; ///< platform-specific thread options

    waitlist_t waiters; ///< list of threads waiting for this thread to exit

    thread_signal_info_t signal_info;

    ~Thread();

    static bool IsValid(const Thread *thread)
    {
        if (auto ptr = thread; ptr)
            return ptr->magic == THREAD_MAGIC_THRD;
        else
            return false;
    }

    friend mos::SyslogStreamWriter operator<<(mos::SyslogStreamWriter stream, const Thread *thread)
    {
        if (!Thread::IsValid(thread))
            return stream << "[invalid]";

        return stream << fmt("[t{}:{}]", thread->tid, thread->name.value_or("<no name>"));
    }
};

/** @} */
