// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/process.h"

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/io/io.h"
#include "mos/mm/mm.h"
#include "mos/tasks/signal.h"

#include <abi-bits/wait.h>
#include <errno.h>
#include <limits.h>
#include <mos/filesystem/dentry.h>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.h>
#include <mos/lib/structures/hashmap.h>
#include <mos/lib/structures/hashmap_common.h>
#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/cow.h>
#include <mos/mm/paging/paging.h>
#include <mos/panic.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/tasks/schedule.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/thread.h>
#include <mos/tasks/wait.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

hashmap_t process_table = { 0 }; // pid_t -> process_t

static const char *vmap_content_str[] = {
    [VMAP_UNKNOWN] = "unknown", //
    [VMAP_HEAP] = "heap",       //
    [VMAP_STACK] = "stack",     //
    [VMAP_FILE] = "file",       //
    [VMAP_MMAP] = "mmap",       //
    [VMAP_DMA] = "DMA",         //
};

const char *vmap_type_str[] = {
    [VMAP_TYPE_SHARED] = "shared",
    [VMAP_TYPE_PRIVATE] = "private",
};

static pid_t new_process_id(void)
{
    static pid_t next = 1;
    return (pid_t){ next++ };
}

process_t *process_allocate(process_t *parent, const char *name)
{
    process_t *proc = kmalloc(process_cache);

    proc->magic = PROCESS_MAGIC_PROC;
    proc->pid = new_process_id();
    linked_list_init(&proc->threads);
    linked_list_init(&proc->children);

    waitlist_init(&proc->signal_info.sigchild_waitlist);

    proc->name = strdup(name ? name : "<unknown>");

    if (likely(parent))
    {
        proc->parent = parent;
    }
    else if (unlikely(proc->pid == 1) || unlikely(proc->pid == 2))
    {
        proc->parent = proc;
        pr_demph(process, "special process %pp created", (void *) proc);
    }
    else
    {
        pr_emerg("process %pp has no parent", (void *) proc);
        kfree(proc->name);
        kfree(proc);
        return NULL;
    }

    list_node_append(&proc->parent->children, list_node(proc));

    if (unlikely(proc->pid == 2))
    {
        proc->mm = platform_info->kernel_mm; // ! Special case: PID 2 (kthreadd) uses the kernel page table
    }
    else
    {
        proc->mm = mm_create_context();
    }

    if (unlikely(!proc->mm))
    {
        pr_emerg("failed to create page table for process %pp", (void *) proc);
        kfree(proc->name);
        kfree(proc);
        return NULL;
    }

    return proc;
}

void process_destroy(process_t *process)
{
    if (!process_is_valid(process))
        return;

    MOS_ASSERT(process != current_process);
    pr_dinfo2(process, "destroying process %pp", (void *) process);

    if (process->name != NULL)
        kfree(process->name);

    if (process->main_thread != NULL)
    {
        hashmap_remove(&thread_table, process->main_thread->tid);
        thread_destroy(process->main_thread);
        process->main_thread = NULL;
    }

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

    kfree(process);
}

process_t *process_new(process_t *parent, const char *name, const stdio_t *ios)
{
    process_t *proc = process_allocate(parent, name);
    if (unlikely(!proc))
        return NULL;
    pr_dinfo2(process, "creating process %pp", (void *) proc);

    process_attach_ref_fd(proc, ios && ios->in ? ios->in : io_null, FD_FLAGS_NONE);
    process_attach_ref_fd(proc, ios && ios->out ? ios->out : io_null, FD_FLAGS_NONE);
    process_attach_ref_fd(proc, ios && ios->err ? ios->err : io_null, FD_FLAGS_NONE);

    proc->main_thread = thread_new(proc, THREAD_MODE_USER, proc->name, 0, NULL);

    vmap_t *heap = cow_allocate_zeroed_pages(proc->mm, 1, MOS_ADDR_USER_HEAP, VALLOC_DEFAULT, VM_USER_RW);
    vmap_finalise_init(heap, VMAP_HEAP, VMAP_TYPE_PRIVATE);

    proc->working_directory = dentry_ref_up_to(parent ? parent->working_directory : root_dentry, root_dentry);

    void *old_proc = hashmap_put(&process_table, proc->pid, proc);
    MOS_ASSERT_X(old_proc == NULL, "process already exists, go and buy yourself a lottery :)");
    return proc;
}

process_t *process_get(pid_t pid)
{
    process_t *p = hashmap_get(&process_table, pid);
    if (process_is_valid(p))
        return p;

    return NULL;
}

fd_t process_attach_ref_fd(process_t *process, io_t *file, fd_flags_t flags)
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

io_t *process_get_fd(process_t *process, fd_t fd)
{
    MOS_ASSERT(process_is_valid(process));
    if (fd < 0 || fd >= MOS_PROCESS_MAX_OPEN_FILES)
        return NULL;
    return process->files[fd].io;
}

bool process_detach_fd(process_t *process, fd_t fd)
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
        list_foreach(process_t, child, current_process->children)
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
    process_t *target_proc = process_get(pid);
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
    hashmap_remove(&process_table, pid);
    process_destroy(target_proc);

    return pid;
}

void process_handle_exit(process_t *process, u8 exit_code, signal_t signal)
{
    MOS_ASSERT(process_is_valid(process));
    pr_dinfo2(process, "process %pp exited with code %d, signal %d", (void *) process, exit_code, signal);

    if (unlikely(process->pid == 1))
        mos_panic("init process terminated with code %d, signal %d", exit_code, signal);

    process->exit_status = W_EXITCODE(exit_code, signal);

    list_foreach(thread_t, thread, process->threads)
    {
        spinlock_acquire(&thread->state_lock);
        if (thread->state == THREAD_STATE_DEAD)
        {
            pr_dinfo2(process, "cleanup thread %pp", (void *) thread);
            MOS_ASSERT(thread != current_thread);
            hashmap_remove(&thread_table, thread->tid);
            thread_destroy(thread);
        }
        else
        {
            // send termination signal to all threads, except the current one
            if (thread != current_thread)
            {
                pr_dinfo2(process, "sending SIGKILL to thread %pp", (void *) thread);
                signal_send_to_thread(thread, SIGKILL);
                spinlock_release(&thread->state_lock);
                thread_wait_for_tid(thread->tid);
                spinlock_acquire(&thread->state_lock);
                pr_dinfo2(process, "thread %pp terminated", (void *) thread);
                MOS_ASSERT(thread->state == THREAD_STATE_DEAD);
                hashmap_remove(&thread_table, thread->tid);
                thread_destroy(thread);
            }
            else
            {
                thread->state = THREAD_STATE_DEAD;
                spinlock_release(&thread->state_lock);
                process->main_thread = thread; // make sure we properly destroy the main thread at the end
                pr_dinfo2(process, "thread %pp is current thread, replacing main thread", (void *) thread);
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
    list_foreach(process_t, child, process->children)
    {
        child->parent = process->parent;
        linked_list_init(list_node(child));
        list_node_append(&process->parent->children, list_node(child));
    }

    pr_dinfo2(process, "closed %zu/%zu files owned by %pp", files_closed, files_total, (void *) process);
    process->exited = true;

    // wake up parent
    pr_dinfo2(process, "waking up parent %pp", (void *) process->parent);
    signal_send_to_process(process->parent, SIGCHLD);
    waitlist_wake(&process->parent->signal_info.sigchild_waitlist, INT_MAX);

    dentry_unref(process->working_directory);
    reschedule();
    MOS_UNREACHABLE();
}

ptr_t process_grow_heap(process_t *process, size_t npages)
{
    MOS_ASSERT(process_is_valid(process));

    vmap_t *heap = NULL;
    list_foreach(vmap_t, mmap, process->mm->mmaps)
    {
        if (mmap->content == VMAP_HEAP)
        {
            heap = mmap;
            spinlock_acquire(&heap->lock);
            break;
        }
    }

    MOS_ASSERT(heap != NULL);

    const ptr_t heap_top = heap->vaddr + heap->npages * MOS_PAGE_SIZE;

    heap->npages += npages; // let the page fault handler do the rest of the allocation
    pr_dinfo2(process, "grew heap of process %pp by %zu pages", (void *) process, npages);
    spinlock_release(&heap->lock);
    return heap_top + npages * MOS_PAGE_SIZE;
}

void process_dump_mmaps(const process_t *process)
{
    pr_info("process %pp:", (void *) process);
    size_t i = 0;
    list_foreach(vmap_t, map, process->mm->mmaps)
    {
        i++;
        const char *typestr = vmap_content_str[map->content];
        const char *forkmode = vmap_type_str[map->type];
        pr_info2("  %3zd: %pvm, %s, %s", i, (void *) map, typestr, forkmode);
    }

    pr_info("total: %zd memory regions", i);
}

bool process_register_signal_handler(process_t *process, signal_t sig, const sigaction_t *sigaction)
{
    pr_dinfo2(signal, "registering signal handler for process %pp, signal %d", (void *) process, sig);
    if (!sigaction)
    {
        process->signal_info.handlers[sig] = (sigaction_t){ .sa_handler = SIG_DFL };
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
    do_print("%s", "name", current_process->name);

    do_print("%d", "parent", current_process->parent->pid);

    list_foreach(thread_t, thread, current_process->threads)
    {
        do_print("%pt", "thread", (void *) thread);
    }
    return true;
}

static bool process_sysfs_thread_stat(sysfs_file_t *f)
{
    do_print("%d", "tid", current_thread->tid);
    do_print("%s", "name", current_thread->name);
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
        sysfs_printf(f, stat_line("%s"), "Type", vmap_type_str[vmap->type]);
        sysfs_printf(f, stat_line("%s"), "Content", vmap_content_str[vmap->content]);
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
