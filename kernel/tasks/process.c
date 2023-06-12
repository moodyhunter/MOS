// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/process.h"

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

static slab_t *vmap_cache = NULL;
MOS_SLAB_AUTOINIT("vmap", vmap_cache, vmap_t);

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
    linked_list_init(&proc->mmaps);

    if (likely(parent))
    {
        proc->parent = parent;
    }
    else if (unlikely(proc->pid == 1) || unlikely(proc->pid == 2))
    {
        proc->parent = proc;
        pr_emph("special process %ld (%s) created", proc->pid, name);
    }
    else
    {
        pr_emerg("process %ld has no parent", proc->pid);
        kfree(proc);
        return NULL;
    }

    proc->name = strdup(name ? name : "<unknown>");

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
        pr_emerg("failed to create page table for process %ld (%s)", proc->pid, proc->name);
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

    pr_info("creating process %ld (%s)", proc->pid, proc->name);
    if (unlikely(!proc))
        return NULL;

    proc->argv = argv;
    process_attach_ref_fd(proc, ios && ios->in ? ios->in : io_null);
    process_attach_ref_fd(proc, ios && ios->out ? ios->out : io_null);
    process_attach_ref_fd(proc, ios && ios->err ? ios->err : io_null);

    thread_new(proc, THREAD_MODE_USER, proc->name, entry, NULL);

    vmblock_t heap = mm_alloc_pages(proc->mm, 1, MOS_ADDR_USER_HEAP, VALLOC_DEFAULT, VM_USER_RW);
    process_attach_mmap(proc, heap, VMTYPE_HEAP, (vmap_flags_t){ 0 });

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
            mos_warn("process %ld has too many open files", process->pid);
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
    mos_debug(process, "process %ld attached thread %ld", process->pid, thread->tid);
    process->threads[process->threads_count++] = thread;
}

void process_attach_mmap(process_t *process, vmblock_t block, vmap_content_t type, vmap_flags_t flags)
{
    MOS_ASSERT(process_is_valid(process));
    MOS_ASSERT(block.address_space == process->mm);

    mos_debug(process, "process %ld attached mmap " PTR_RANGE, process->pid, block.vaddr, block.vaddr + block.npages * MOS_PAGE_SIZE);

    vmap_t *map = kmemcache_alloc(vmap_cache);
    linked_list_init(list_node(map));
    map->blk = block;
    map->content = type;
    map->flags = flags;

    list_node_append(&process->mmaps, list_node(map));
}

void process_detach_mmap(process_t *process, vmblock_t block)
{
    MOS_ASSERT(process_is_valid(process));
    list_foreach(vmap_t, map, process->mmaps)
    {
        if (map->blk.vaddr != block.vaddr)
            continue;

        spinlock_acquire(&map->lock);
        list_remove(map);
        MOS_ASSERT(map->blk.npages == block.npages);
        mm_unmap_pages(process->mm, block.vaddr, block.npages);
        kfree(map);
        return;
    }

    mos_warn("process %ld tried to detach a non-existent mmap", process->pid);
}

bool process_wait_for_pid(pid_t pid)
{
    process_t *target = process_get(pid);
    if (target == NULL)
    {
        pr_warn("process %ld does not exist", pid);
        return false;
    }

    bool ok = reschedule_for_waitlist(&target->waiters);
    MOS_UNUSED(ok);

    return true;
}

void process_handle_exit(process_t *process, int exit_code)
{
    MOS_ASSERT(process_is_valid(process));
    pr_info("process %ld exited with code %d", process->pid, exit_code);

    mos_debug(process, "terminating all %lu threads owned by %ld", process->threads_count, process->pid);
    for (int i = 0; i < process->threads_count; i++)
    {
        // TODO: support signals for proper thread termination
        thread_t *thread = process->threads[i];
        spinlock_acquire(&thread->state_lock);
        if (thread->state == THREAD_STATE_DEAD)
        {
            mos_debug(process, "thread %ld is already dead", thread->tid);
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

    mos_debug(process, "closed %zu/%zu files owned by %ld", files_closed, files_total, process->pid);

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

    list_foreach(vmap_t, map, process->mmaps)
    {
        spinlock_acquire(&map->lock);
        list_remove(map);

        mm_unmap_pages(process->mm, map->blk.vaddr, map->blk.npages);
        kfree(map);
    }
}

ptr_t process_grow_heap(process_t *process, size_t npages)
{
    MOS_ASSERT(process_is_valid(process));

    vmap_t *heap = NULL;
    list_foreach(vmap_t, mmap, process->mmaps)
    {
        if (mmap->content == VMTYPE_HEAP)
        {
            heap = mmap;
            spinlock_acquire(&heap->lock);
            break;
        }
    }

    MOS_ASSERT(heap != NULL);

    const ptr_t heap_top = heap->blk.vaddr + heap->blk.npages * MOS_PAGE_SIZE;

    if (heap->flags.cow)
    {
        vmblock_t zeroed = mm_alloc_zeroed_pages(process->mm, npages, heap_top, VALLOC_EXACT, VM_USER_RW);
        MOS_ASSERT(zeroed.npages == npages);
    }
    else
    {
        vmblock_t new_part = mm_alloc_pages(process->mm, npages, heap_top, VALLOC_EXACT, VM_USER_RW);
        MOS_ASSERT(new_part.npages == npages);
    }

    mos_debug(process, "grew heap of process %ld by %zu pages", process->pid, npages);
    heap->blk.npages += npages;
    spinlock_release(&heap->lock);
    return heap_top + npages * MOS_PAGE_SIZE;
}

void process_dump_mmaps(const process_t *process)
{
    pr_info("process %ld (%s):", process->pid, process->name);
    size_t i = 0;
    list_foreach(vmap_t, map, process->mmaps)
    {
        i++;
        const char *typestr = "<unknown>";
        switch (map->content)
        {
            case VMTYPE_CODE: typestr = "code"; break;
            case VMTYPE_DATA: typestr = "data"; break;
            case VMTYPE_HEAP: typestr = "heap"; break;
            case VMTYPE_STACK: typestr = "stack"; break;
            case VMTYPE_KSTACK: typestr = "stack (kernel)"; break;
            case VMTYPE_FILE: typestr = "file"; break;
            case VMTYPE_MMAP: typestr = "mmap"; break;
            default: mos_warn("unknown memory region type %x", map->content);
        };

        char forkmode = '-';
        if (map->content == VMTYPE_FILE || map->content == VMTYPE_MMAP)
        {
            switch (map->flags.fork_mode)
            {
                case VMAP_FORK_NA: forkmode = '!'; break;
                case VMAP_FORK_SHARED: forkmode = 's'; break;
                case VMAP_FORK_PRIVATE: forkmode = 'p'; break;
                default: mos_warn("unknown fork mode %x", map->flags.fork_mode);
            }
        }

        pr_info("  %3zd: " PTR_FMT ", %5zd page(s), [%c%c%c%c%c%c, %c%c]: %s",
                i,                                              //
                map->blk.vaddr,                                 //
                map->blk.npages,                                //
                map->blk.flags & VM_READ ? 'r' : '-',           //
                map->blk.flags & VM_WRITE ? 'w' : '-',          //
                map->blk.flags & VM_EXEC ? 'x' : '-',           //
                map->blk.flags & VM_GLOBAL ? 'g' : '-',         //
                map->blk.flags & VM_USER ? 'u' : '-',           //
                map->blk.flags & VM_CACHE_DISABLED ? 'C' : '-', //
                map->flags.cow ? 'c' : '-',                     //
                forkmode,                                       //
                typestr                                         //
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
