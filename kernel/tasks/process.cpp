// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/process.hpp"

#include "mos/filesystem/sysfs/sysfs.hpp"
#include "mos/filesystem/sysfs/sysfs_autoinit.hpp"
#include "mos/io/io.hpp"
#include "mos/mm/mm.hpp"
#include "mos/tasks/signal.hpp"

#include <abi-bits/wait.h>
#include <errno.h>
#include <limits.h>
#include <mos/filesystem/dentry.hpp>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.hpp>
#include <mos/hashmap.hpp>
#include <mos/lib/structures/hashmap.hpp>
#include <mos/lib/structures/hashmap_common.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/misc/panic.hpp>
#include <mos/mm/cow.hpp>
#include <mos/mm/paging/paging.hpp>
#include <mos/platform/platform.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/schedule.hpp>
#include <mos/tasks/task_types.hpp>
#include <mos/tasks/thread.hpp>
#include <mos/tasks/wait.hpp>
#include <mos/type_utils.hpp>
#include <mos_stdio.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

mos::HashMap<pid_t, Process *> ProcessTable;

const char *get_vmap_content_str(vmap_content_t content)
{
    switch (content)
    {
        case VMAP_UNKNOWN: return "unknown";
        case VMAP_STACK: return "stack";
        case VMAP_FILE: return "file";
        case VMAP_MMAP: return "mmap";
        case VMAP_DMA: return "DMA";
        default: return "unknown";
    };
}

const char *get_vmap_type_str(vmap_type_t type)
{
    switch (type)
    {
        case VMAP_TYPE_PRIVATE: return "private";
        case VMAP_TYPE_SHARED: return "shared";
        default: return "unknown";
    };
}

static pid_t new_process_id(void)
{
    static std::atomic<pid_t> next = 1;
    return next++;
}

Process::Process(Private, Process *const parent_, mos::string_view name_) : magic(PROCESS_MAGIC_PROC), pid(new_process_id()), parent(parent_)
{
    linked_list_init(&threads);
    linked_list_init(&children);

    waitlist_init(&signal_info.sigchild_waitlist);

    this->name = name_.empty() ? "<unknown>" : name_;

    if (unlikely(pid == 1) || unlikely(pid == 2))
    {
        this->parent = this;
        pr_demph(process, "special process %pp created", (void *) this);
    }

    if (parent == nullptr)
        mos_panic("process %pp has no parent", (void *) this);

    list_node_append(&parent->children, list_node(this));

    if (unlikely(pid == 2))
    {
        mm = platform_info->kernel_mm; // ! Special case: PID 2 (kthreadd) uses the kernel page table
    }
    else
    {
        mm = mm_create_context();
    }

    MOS_ASSERT_X(mm != NULL, "failed to create page table for process");
}

void process_destroy(Process *process)
{
    if (!process_is_valid(process))
        return;

    ProcessTable.remove(process->pid);

    MOS_ASSERT(process != current_process);
    pr_dinfo2(process, "destroying process %pp", (void *) process);

    MOS_ASSERT_X(process->main_thread != NULL, "main thread must be dead before destroying process");

    spinlock_acquire(&process->main_thread->state_lock);
    thread_destroy(process->main_thread);
    process->main_thread = NULL;

    if (process->mm != NULL)
    {
        spinlock_acquire(&process->mm->mm_lock);
        list_foreach(vmap_t, vmap, process->mm->mmaps)
        {
            spinlock_acquire(&vmap->lock);
            vmap_destroy(vmap);
        }

        // free page table
        MOS_ASSERT(process->mm != current_mm);
        mm_destroy_context(process->mm);
    }

    delete process;
}

Process *process_new(Process *parent, mos::string_view name, const stdio_t *ios)
{
    const auto proc = Process::New(parent, name);
    if (unlikely(!proc))
        return NULL;
    pr_dinfo2(process, "creating process %pp", (void *) proc);

    process_attach_ref_fd(proc, ios && ios->in ? ios->in : io_null, FD_FLAGS_NONE);
    process_attach_ref_fd(proc, ios && ios->out ? ios->out : io_null, FD_FLAGS_NONE);
    process_attach_ref_fd(proc, ios && ios->err ? ios->err : io_null, FD_FLAGS_NONE);

    auto thread = thread_new(proc, THREAD_MODE_USER, proc->name, 0, NULL);
    if (thread.isErr())
    {
        process_destroy(proc);
        return NULL; // TODO
    }
    proc->main_thread = thread.get();
    proc->working_directory = dentry_ref_up_to(parent ? parent->working_directory : root_dentry, root_dentry);

    ProcessTable.insert(proc->pid, proc);
    return proc;
}

Process *process_get(pid_t pid)
{
    const auto pproc = ProcessTable.get(pid);
    if (!pproc)
    {
        pr_warn("process %d does not exist", pid);
        return nullptr;
    }

    if (process_is_valid(**pproc))
        return **pproc;

    return NULL;
}

fd_t process_attach_ref_fd(Process *process, io_t *file, fd_flags_t flags)
{
    MOS_ASSERT(process_is_valid(process));

    // find a free fd
    fd_t fd = 0;
    while (process->files[fd].io)
    {
        fd++;
        if (fd >= MOS_PROCESS_MAX_OPEN_FILES)
        {
            mos_warn("process %pp has too many open files", (void *) process);
            return -EMFILE;
        }
    }

    process->files[fd].io = io_ref(file);
    process->files[fd].flags = flags;
    return fd;
}

io_t *process_get_fd(Process *process, fd_t fd)
{
    MOS_ASSERT(process_is_valid(process));
    if (fd < 0 || fd >= MOS_PROCESS_MAX_OPEN_FILES)
        return NULL;
    return process->files[fd].io;
}

bool process_detach_fd(Process *process, fd_t fd)
{
    MOS_ASSERT(process_is_valid(process));
    if (fd < 0 || fd >= MOS_PROCESS_MAX_OPEN_FILES)
        return false;
    io_t *io = process->files[fd].io;

    if (unlikely(!io_valid(io)))
        return false;

    io_unref(process->files[fd].io);
    process->files[fd] = nullfd;
    return true;
}

pid_t process_wait_for_pid(pid_t pid, u32 *exit_code, u32 flags)
{
    if (pid == -1)
    {
        if (list_is_empty(&current_process->children))
            return -ECHILD; // no children to wait for at all

    find_dead_child:;
        // first find if there are any dead children
        list_foreach(Process, child, current_process->children)
        {
            if (child->exited)
            {
                pid = child->pid;
                goto child_dead;
            }
        }

        if (flags & WNOHANG)
            return 0; // no dead children, and we don't want to wait

        // we have to wait for a child to die
        MOS_ASSERT_X(!current_process->signal_info.sigchild_waitlist.closed, "waitlist is in use");

        const bool ok = reschedule_for_waitlist(&current_process->signal_info.sigchild_waitlist);
        MOS_ASSERT(ok); // we just created the waitlist, it should be empty

        // we are woken up by a signal, or a child dying
        if (signal_has_pending())
        {
            pr_dinfo2(process, "woken up by signal");
            waitlist_remove_me(&current_process->signal_info.sigchild_waitlist);
            return -ERESTARTSYS;
        }

        goto find_dead_child;
    }

child_dead:;
    Process *target_proc = process_get(pid);
    if (target_proc == NULL)
    {
        pr_warn("process %d does not exist", pid);
        return -ECHILD;
    }

    while (true)
    {
        // child is already dead
        if (target_proc->exited)
            break;

        // wait for the child to die
        bool ok = reschedule_for_waitlist(&current_process->signal_info.sigchild_waitlist);
        MOS_ASSERT(ok);
    }

    list_remove(target_proc); // remove from parent's children list
    pid = target_proc->pid;
    if (exit_code)
        *exit_code = target_proc->exit_status;
    process_destroy(target_proc);

    return pid;
}

void process_exit(Process *process, u8 exit_code, signal_t signal)
{
    MOS_ASSERT(process_is_valid(process));
    pr_dinfo2(process, "process %pp exited with code %d, signal %d", (void *) process, exit_code, signal);

    if (unlikely(process->pid == 1))
        mos_panic("init process terminated with code %d, signal %d", exit_code, signal);

    list_foreach(Thread, thread, process->threads)
    {
        spinlock_acquire(&thread->state_lock);
        if (thread->state == THREAD_STATE_DEAD)
        {
            pr_dinfo2(process, "cleanup thread %pt", (void *) thread);
            MOS_ASSERT(thread != current_thread);
            thread_table.remove(thread->tid);
            list_remove(thread);
            thread_destroy(thread);
        }
        else
        {
            // send termination signal to all threads, except the current one
            if (thread != current_thread)
            {
                pr_dinfo2(signal, "sending SIGKILL to thread %pt", (void *) thread);
                spinlock_release(&thread->state_lock);
                signal_send_to_thread(thread, SIGKILL);
                thread_wait_for_tid(thread->tid);
                spinlock_acquire(&thread->state_lock);
                pr_dinfo2(process, "thread %pt terminated", (void *) thread);
                MOS_ASSERT_X(thread->state == THREAD_STATE_DEAD, "thread %pt is not dead", (void *) thread);
                thread_table.remove(thread->tid);
                thread_destroy(thread);
            }
            else
            {
                spinlock_release(&thread->state_lock);
                process->main_thread = thread; // make sure we properly destroy the main thread at the end
                pr_dinfo2(process, "thread %pt is current thread, making it main thread", (void *) thread);
            }
        }
    }

    size_t files_total = 0;
    size_t files_closed = 0;
    for (int i = 0; i < MOS_PROCESS_MAX_OPEN_FILES; i++)
    {
        fd_type file = process->files[i];
        process->files[i] = nullfd;

        if (io_valid(file.io))
        {
            files_total++;
            if (io_unref(file.io) == NULL)
                files_closed++;
        }
    }

    // re-parent all children to parent of this process
    list_foreach(Process, child, process->children)
    {
        child->parent = process->parent;
        linked_list_init(list_node(child));
        list_node_append(&process->parent->children, list_node(child));
    }

    dentry_unref(process->working_directory);

    pr_dinfo2(process, "closed %zu/%zu files owned by %pp", files_closed, files_total, (void *) process);
    process->exited = true;
    process->exit_status = W_EXITCODE(exit_code, signal);

    // let the parent wait for our exit
    spinlock_acquire(&current_thread->state_lock);

    // wake up parent
    pr_dinfo2(process, "waking up parent %pp", (void *) process->parent);
    signal_send_to_process(process->parent, SIGCHLD);
    waitlist_wake(&process->parent->signal_info.sigchild_waitlist, INT_MAX);

    thread_exit_locked(current_thread);
    MOS_UNREACHABLE();
}

void process_dump_mmaps(const Process *process)
{
    pr_info("process %pp:", (void *) process);
    size_t i = 0;
    list_foreach(vmap_t, map, process->mm->mmaps)
    {
        i++;
        const char *typestr = get_vmap_content_str(map->content);
        const char *forkmode = get_vmap_type_str(map->type);
        pr_info2("  %3zd: %pvm, %s, %s", i, (void *) map, typestr, forkmode);
    }

    pr_info("total: %zd memory regions", i);
}

bool process_register_signal_handler(Process *process, signal_t sig, const sigaction_t *sigaction)
{
    pr_dinfo2(signal, "registering signal handler for process %pp, signal %d", (void *) process, sig);
    if (!sigaction)
    {
        process->signal_info.handlers[sig] = (sigaction_t) { .handler = SIG_DFL };
        return true;
    }
    process->signal_info.handlers[sig] = *sigaction;
    return true;
}

// ! sysfs support

#define do_print(fmt, name, item) sysfs_printf(f, "%-10s: " fmt "\n", name, item);

static bool process_sysfs_process_stat(sysfs_file_t *f)
{
    do_print("%d", "pid", current_process->pid);
    do_print("%s", "name", current_process->name.c_str());

    do_print("%d", "parent", current_process->parent->pid);

    list_foreach(Thread, thread, current_process->threads)
    {
        do_print("%pt", "thread", (void *) thread);
    }
    return true;
}

static bool process_sysfs_thread_stat(sysfs_file_t *f)
{
    do_print("%d", "tid", current_thread->tid);
    do_print("%s", "name", current_thread->name.c_str());
    return true;
}

static bool process_sysfs_vmap_stat(sysfs_file_t *f)
{
    list_foreach(vmap_t, vmap, current_mm->mmaps)
    {
#define stat_line(fmt) "   %9s: " fmt "\n"
        sysfs_printf(f, PTR_RANGE "\n", vmap->vaddr, vmap->vaddr + vmap->npages * MOS_PAGE_SIZE);
        sysfs_printf(f, stat_line("%pvf,%s%s"), "Perms",          //
                     (void *) &vmap->vmflags,                     //
                     vmap->vmflags & VM_USER ? "user" : "kernel", //
                     vmap->vmflags & VM_GLOBAL ? " global" : ""   //
        );
        sysfs_printf(f, stat_line("%s"), "Type", get_vmap_type_str(vmap->type));
        sysfs_printf(f, stat_line("%s"), "Content", get_vmap_content_str(vmap->content));
        if (vmap->content == VMAP_FILE)
        {
            char filepath[MOS_PATH_MAX_LENGTH];
            io_get_name(vmap->io, filepath, sizeof(filepath));
            sysfs_printf(f, stat_line("%s"), "  File", filepath);
            sysfs_printf(f, stat_line("%zu bytes"), "  Offset", vmap->io_offset);
        }
        sysfs_printf(f, stat_line("%zu pages"), "Total", vmap->npages);
        sysfs_printf(f, stat_line("%zu pages"), "Regular", vmap->stat.regular);
        sysfs_printf(f, stat_line("%zu pages"), "PageCache", vmap->stat.pagecache);
        sysfs_printf(f, stat_line("%zu pages"), "CoW", vmap->stat.cow);
#undef stat_line
        sysfs_printf(f, "\n");
    }
    return true;
}

static sysfs_item_t process_sysfs_items[] = {
    SYSFS_RO_ITEM("process", process_sysfs_process_stat),
    SYSFS_RO_ITEM("thread", process_sysfs_thread_stat),
    SYSFS_RO_ITEM("vmaps", process_sysfs_vmap_stat),
};

SYSFS_AUTOREGISTER(current, process_sysfs_items);
