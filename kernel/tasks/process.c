// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/process.h"

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/mm/mm.h"
#include "mos/mm/slab_autoinit.h"

#include <mos/filesystem/dentry.h>
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
#include <stdlib.h>
#include <string.h>

hashmap_t process_table = { 0 }; // pid_t -> process_t

static const char *vmap_content_str[] = {
    [VMAP_UNKNOWN] = "unknown", [VMAP_CODE] = "code", [VMAP_DATA] = "data", [VMAP_HEAP] = "heap", [VMAP_STACK] = "stack", [VMAP_FILE] = "file", [VMAP_MMAP] = "mmap",
};

const char *vmap_fork_behavior_str[] = {
    [VMAP_FORK_SHARED] = "shared",
    [VMAP_FORK_PRIVATE] = "private",
};

static pid_t new_process_id(void)
{
    static pid_t next = 1;
    return (pid_t){ next++ };
}

process_t *process_allocate(process_t *parent, const char *name)
{
    process_t *proc = kmemcache_alloc(process_cache);

    proc->magic = PROCESS_MAGIC_PROC;
    proc->pid = new_process_id();

    waitlist_init(&proc->waiters);

    proc->name = strdup(name ? name : "<unknown>");

    if (likely(parent))
    {
        proc->parent = parent;
    }
    else if (unlikely(proc->pid == 1) || unlikely(proc->pid == 2))
    {
        proc->parent = proc;
        pr_emph("special process %pp created", (void *) proc);
    }
    else
    {
        pr_emerg("process %pp has no parent", (void *) proc);
        kfree(proc->name);
        kfree(proc);
        return NULL;
    }

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

process_t *process_new(process_t *parent, const char *name, const stdio_t *ios, thread_entry_t entry, argv_t argv)
{
    process_t *proc = process_allocate(parent, name);
    if (unlikely(!proc))
        return NULL;

    mos_debug(process, "creating process %pp", (void *) proc);
    if (unlikely(!proc))
        return NULL;

    proc->argv = argv;
    process_attach_ref_fd(proc, ios && ios->in ? ios->in : io_null);
    process_attach_ref_fd(proc, ios && ios->out ? ios->out : io_null);
    process_attach_ref_fd(proc, ios && ios->err ? ios->err : io_null);

    thread_new(proc, THREAD_MODE_USER, proc->name, entry, NULL);

    vmap_t *heap = mm_alloc_pages(proc->mm, 1, MOS_ADDR_USER_HEAP, VALLOC_DEFAULT, VM_USER_RW);
    vmap_finalise_init(heap, VMAP_HEAP, VMAP_FORK_PRIVATE);

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

fd_t process_attach_ref_fd(process_t *process, io_t *file)
{
    MOS_ASSERT(process_is_valid(process));

    // find a free fd
    fd_t fd = 0;
    while (process->files[fd] != NULL)
    {
        fd++;
        if (fd >= MOS_PROCESS_MAX_OPEN_FILES)
        {
            mos_warn("process %pp has too many open files", (void *) process);
            return -1;
        }
    }

    process->files[fd] = io_ref(file);
    return fd;
}

io_t *process_get_fd(process_t *process, fd_t fd)
{
    MOS_ASSERT(process_is_valid(process));
    if (fd < 0 || fd >= MOS_PROCESS_MAX_OPEN_FILES)
        return NULL;
    return process->files[fd];
}

bool process_detach_fd(process_t *process, fd_t fd)
{
    MOS_ASSERT(process_is_valid(process));
    if (fd < 0 || fd >= MOS_PROCESS_MAX_OPEN_FILES)
        return false;
    io_unref(process->files[fd]);
    process->files[fd] = NULL;
    return true;
}

void process_attach_thread(process_t *process, thread_t *thread)
{
    MOS_ASSERT(process_is_valid(process));
    MOS_ASSERT(thread_is_valid(thread));
    MOS_ASSERT(thread->owner == process);
    mos_debug(process, "process %pp attached thread %pt", (void *) process, (void *) thread);
    MOS_ASSERT(process->threads_count < MOS_PROCESS_MAX_THREADS);
    MOS_ASSERT(process->threads[process->threads_count] == NULL);
    process->threads[process->threads_count++] = thread;
}

bool process_wait_for_pid(pid_t pid)
{
    process_t *target = process_get(pid);
    if (target == NULL)
    {
        pr_warn("process %d does not exist", pid);
        return false;
    }

    bool ok = reschedule_for_waitlist(&target->waiters);
    MOS_UNUSED(ok);

    return true;
}

void process_handle_exit(process_t *process, int exit_code)
{
    MOS_ASSERT(process_is_valid(process));
    mos_debug(process, "process %pp exited with code %d", (void *) process, exit_code);

    mos_debug(process, "terminating all %lu threads owned by %pp", process->threads_count, (void *) process);
    for (int i = 0; i < process->threads_count; i++)
    {
        // TODO: support signals for proper thread termination
        thread_t *thread = process->threads[i];
        spinlock_acquire(&thread->state_lock);
        if (thread->state == THREAD_STATE_DEAD)
        {
            mos_debug(process, "thread %pt is already dead", (void *) thread);
        }
        else
        {
            thread->state = THREAD_STATE_DEAD; // cleanup will be done by the scheduler
        }
        spinlock_release(&thread->state_lock);
    }

    size_t files_total = 0;
    size_t files_closed = 0;
    for (int i = 0; i < MOS_PROCESS_MAX_OPEN_FILES; i++)
    {
        io_t *file = process->files[i];
        process->files[i] = NULL;

        if (io_valid(file))
        {
            files_total++;
            if (io_unref(file) == NULL)
                files_closed++;
        }
    }

    mos_debug(process, "closed %zu/%zu files owned by %pp", files_closed, files_total, (void *) process);

    dentry_unref(process->working_directory);

    waitlist_close(&process->waiters);
    waitlist_wake(&process->waiters, INT_MAX);

    reschedule();
    MOS_UNREACHABLE();
}

void process_handle_cleanup(process_t *process)
{
    MOS_ASSERT(process_is_valid(process));
    MOS_ASSERT_X(current_process != process, "cannot cleanup current process");

    list_foreach(vmap_t, map, process->mm->mmaps)
    {
        spinlock_acquire(&map->lock);
        list_remove(map);

        mm_unmap_pages(process->mm, map->vaddr, map->npages);
        kfree(map);
    }
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
    mos_debug(process, "grew heap of process %pp by %zu pages", (void *) process, npages);
    spinlock_release(&heap->lock);
    return heap_top + npages * MOS_PAGE_SIZE;
}

void process_dump_mmaps(const process_t *process)
{
    pr_info("process %p:", (void *) process);
    size_t i = 0;
    list_foreach(vmap_t, map, process->mm->mmaps)
    {
        i++;
        const char *typestr = vmap_content_str[map->content];
        const char *forkmode = vmap_fork_behavior_str[map->fork_behavior];
        pr_info("  %3zd: " PTR_FMT ", %5zd page(s), [%pvf%c%c, %s]: %s",
                i,                                    //
                map->vaddr,                           //
                map->npages,                          //
                (void *) &map->vmflags,               //
                map->vmflags & VM_GLOBAL ? 'g' : '-', //
                map->vmflags & VM_USER ? 'u' : '-',   //
                forkmode,                             //
                typestr                               //
        );
    }

    pr_info("total: %zd memory regions", i);
}

bool process_register_signal_handler(process_t *process, signal_t sig, signal_action_t *sigaction)
{
    // stub
    MOS_UNUSED(process);
    MOS_UNUSED(sig);
    MOS_UNUSED(sigaction);
    return false;
}

// ! sysfs support

#define do_print(fmt, name, item) sysfs_printf(f, "%-10s: " fmt "\n", name, item);

static bool process_sysfs_process_stat(sysfs_file_t *f)
{
    do_print("%d", "pid", current_process->pid);
    do_print("%s", "name", current_process->name);

    do_print("%d", "parent", current_process->parent->pid);
    do_print("%zd", "n_threads", current_process->threads_count);
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
#define stat_line(fmt) "   %8s: " fmt "\n"
        sysfs_printf(f,
                     PTR_RANGE " [%pvf] %s %s%s\n"                //
                     stat_line("%s")                              //
                     stat_line("%zu pages")                       //
                     stat_line("%zu pages")                       //
                     stat_line("%zu pages"),                      //
                     vmap->vaddr,                                 //
                     vmap->vaddr + vmap->npages * MOS_PAGE_SIZE,  //
                     (void *) &vmap->vmflags,                     //
                     vmap_fork_behavior_str[vmap->fork_behavior], //
                     vmap->vmflags & VM_USER ? "user" : "kernel", //
                     vmap->vmflags & VM_GLOBAL ? " global" : "",  //
                     "Content", vmap_content_str[vmap->content],  //
                     "Total", vmap->npages,                       //
                     "InMem", vmap->stat.n_inmem,                 //
                     "CoW", vmap->stat.n_cow                      //
        );
#undef stat_line
    }
    return true;
}

static sysfs_item_t process_sysfs_items[] = {
    SYSFS_RO_ITEM("process", process_sysfs_process_stat),
    SYSFS_RO_ITEM("thread", process_sysfs_thread_stat),
    SYSFS_RO_ITEM("vmaps", process_sysfs_vmap_stat),
    SYSFS_END_ITEM,
};

SYSFS_AUTOREGISTER(current, process_sysfs_items);
