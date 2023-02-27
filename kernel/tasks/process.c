// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/process.h"

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "lib/structures/hashmap_common.h"
#include "lib/sync/spinlock.h"
#include "mos/filesystem/vfs.h"
#include "mos/io/terminal.h"
#include "mos/mm/cow.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/memops.h"
#include "mos/mm/paging/paging.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/task_types.h"
#include "mos/tasks/thread.h"

#define PROCESS_HASHTABLE_SIZE 512

hashmap_t *process_table = { 0 }; // pid_t -> process_t

static hash_t process_hash(uintn key)
{
    return (hash_t){ .hash = key };
}

static pid_t new_process_id(void)
{
    static pid_t next = 1;
    return (pid_t){ next++ };
}

static void debug_dump_process(void)
{
    if (current_thread)
    {
        process_t *proc = current_process;
        printk("process %ld (%s) ", proc->pid, proc->name);
        if (proc->parent)
            printk("parent %ld (%s) ", proc->parent->pid, proc->parent->name);
        else
            printk("parent <none> ");
        process_dump_mmaps(proc);
    }
}

process_t *process_allocate(process_t *parent, const char *name)
{
    process_t *proc = kzalloc(sizeof(process_t));

    proc->magic = PROCESS_MAGIC_PROC;
    proc->pid = new_process_id();

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
        proc->pagetable = platform_info->kernel_pgd; // ! Special case: PID 2 (kthreadd) uses the kernel page table
    }
    else
    {
        proc->pagetable = mm_create_user_pgd();
    }

    if (unlikely(!proc->pagetable.pgd))
    {
        pr_emerg("failed to create page table for process %ld (%s)", proc->pid, proc->name);
        kfree(proc->name);
        kfree(proc);
        return NULL;
    }

    return proc;
}

void process_init(void)
{
    process_table = kzalloc(sizeof(hashmap_t));
    hashmap_init(process_table, PROCESS_HASHTABLE_SIZE, process_hash, hashmap_simple_key_compare);
    declare_panic_hook(debug_dump_process);
    install_panic_hook(&debug_dump_process_holder);
}

void process_deinit(void)
{
    hashmap_deinit(process_table);
    kfree(process_table);
}

process_t *process_new(process_t *parent, const char *name, terminal_t *term, thread_entry_t entry, argv_t argv)
{
    process_t *proc = process_allocate(parent, name);
    if (unlikely(!proc))
        return NULL;

    if (unlikely(!term))
    {
        if (likely(parent))
            proc->terminal = parent->terminal;
        else
            mos_panic("init process has no terminal");
    }

    proc->argv = argv;
    proc->terminal = term;
    process_attach_ref_fd(proc, &term->io);
    process_attach_ref_fd(proc, &term->io);
    process_attach_ref_fd(proc, &term->io);

    thread_new(proc, THREAD_MODE_USER, proc->name, entry, NULL);

    vmblock_t heap = mm_alloc_pages(proc->pagetable, 1, PGALLOC_HINT_UHEAP, VM_USER_RW);
    process_attach_mmap(proc, heap, VMTYPE_HEAP, VMBLOCK_DEFAULT);

    proc->working_directory = parent ? parent->working_directory : root_dentry;

    void *old_proc = hashmap_put(process_table, proc->pid, proc);
    MOS_ASSERT_X(old_proc == NULL, "process already exists, go and buy yourself a lottery :)");
    return proc;
}

process_t *process_get(pid_t pid)
{
    process_t *p = hashmap_get(process_table, pid);
    if (p == NULL)
        mos_warn("process %ld not found", pid);
    return p;
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

void process_attach_mmap(process_t *process, vmblock_t block, vmblock_content_t type, vmblock_flags_t flags)
{
    MOS_ASSERT(process_is_valid(process));
    pr_info2("process %ld attached mmap " PTR_FMT "-" PTR_FMT, process->pid, block.vaddr, block.vaddr + block.npages * MOS_PAGE_SIZE);
    process->mmaps = krealloc(process->mmaps, sizeof(proc_vmblock_t) * (process->mmaps_count + 1));
    process->mmaps[process->mmaps_count++] = (proc_vmblock_t){
        .blk = block,
        .content = type,
        .flags = flags,
        .lock = SPINLOCK_INIT,
    };
}

void process_detach_mmap(process_t *process, vmblock_t block)
{
    MOS_ASSERT(process_is_valid(process));
    for (size_t i = 0; i < process->mmaps_count; i++)
    {
        // find the mmap
        if (process->mmaps[i].blk.vaddr == block.vaddr)
        {
            MOS_ASSERT(process->mmaps[i].blk.npages == block.npages);

            // remove it
            process->mmaps_count--;
            process->mmaps[i] = process->mmaps[process->mmaps_count];
            process->mmaps = krealloc(process->mmaps, sizeof(proc_vmblock_t) * process->mmaps_count);

            if (process->mmaps[i].flags & VMBLOCK_COW_ENABLED)
            {
                // TODO CoW tracking
                mm_unmap_pages(process->pagetable, block.vaddr, block.npages);
            }
            else
            {
                // free the pages
                mm_free_pages(process->pagetable, block);
            }

            return;
        }
    }
    mos_warn("process %ld tried to detach a non-existent mmap", process->pid);
}

void process_handle_exit(process_t *process, int exit_code)
{
    MOS_ASSERT(process_is_valid(process));
    pr_info("process %ld exited with code %d", process->pid, exit_code);

    mos_debug(process, "terminating all %lu threads owned by %ld", process->threads_count, process->pid);
    for (int i = 0; i < process->threads_count; i++)
    {
        // TODO: if a thread is being executed, we should wait for it to finish
        // TODO: if a thread holds a lock, we should release it?
        thread_t *thread = process->threads[i];
        spinlock_acquire(&thread->state_lock);
        if (thread->state == THREAD_STATE_DEAD)
        {
            pr_warn("thread %ld is already dead", thread->tid);
        }
        else
        {
            thread->state = THREAD_STATE_DEAD; // cleanup will be done by the scheduler
        }
        spinlock_release(&thread->state_lock);
    }

    size_t files_total = 0;
    size_t files_already_closed = 0;
    for (int i = 0; i < MOS_PROCESS_MAX_OPEN_FILES; i++)
    {
        if (process->files[i])
        {
            files_total++;
            if (!process->files[i]->closed)
            {
                files_already_closed++;
                io_unref(process->files[i]);
            }
            process->files[i] = NULL;
        }
    }

    mos_debug(process, "closed %zu/%zu files owned by %ld", files_already_closed, files_total, process->pid);
}

void process_handle_cleanup(process_t *process)
{
    MOS_ASSERT(process_is_valid(process));
    MOS_ASSERT_X(current_process != process, "cannot cleanup current process");

    mos_debug(process, "unmapping all %zu memory regions owned by %ld", process->mmaps_count, process->pid);
    for (size_t i = 0; i < process->mmaps_count; i++)
    {
        const vmblock_flags_t flags = process->mmaps[i].flags;
        const vmblock_t block = process->mmaps[i].blk;

        // they will be unmapped when the last process detaches them
        // !! TODO: How to track this??
        if (flags & VMBLOCK_COW_ENABLED)
        {
            mm_unmap_pages(process->pagetable, block.vaddr, block.npages);
        }
        else
        {
            mm_free_pages(process->pagetable, block);
        }
    }
}

uintptr_t process_grow_heap(process_t *process, size_t npages)
{
    MOS_ASSERT(process_is_valid(process));

    proc_vmblock_t *heap = NULL;
    for (size_t i = 0; i < process->mmaps_count; i++)
    {
        if (process->mmaps[i].content == VMTYPE_HEAP)
        {
            heap = &process->mmaps[i];
            spinlock_acquire(&heap->lock);
            break;
        }
    }

    MOS_ASSERT(heap != NULL);

    const uintptr_t heap_top = heap->blk.vaddr + heap->blk.npages * MOS_PAGE_SIZE;

    if (heap->flags & VMBLOCK_COW_ENABLED)
    {
        vmblock_t zeroed = mm_alloc_zeroed_pages_at(process->pagetable, heap_top, npages, VM_USER_RW);
        MOS_ASSERT(zeroed.npages == npages);
    }
    else
    {
        vmblock_t new_part = mm_alloc_pages_at(process->pagetable, heap_top, npages, VM_USER_RW);
        if (new_part.vaddr == 0 || new_part.npages != npages)
        {
            mos_warn("failed to grow heap of process %ld", process->pid);
            mm_free_pages(process->pagetable, new_part);
            spinlock_release(&heap->lock);
            return heap_top;
        }
    }

    pr_info2("grew heap of process %ld by %zu pages", process->pid, npages);
    heap->blk.npages += npages;
    spinlock_release(&heap->lock);
    return heap_top + npages * MOS_PAGE_SIZE;
}

void process_dump_mmaps(const process_t *process)
{
    pr_info("process %ld (%s) has %zu memory regions:", process->pid, process->name, process->mmaps_count);
    for (size_t i = 0; i < process->mmaps_count; i++)
    {
        proc_vmblock_t block = process->mmaps[i];
        const char *typestr = "<unknown>";
        switch (block.content)
        {
            case VMTYPE_CODE: typestr = "code"; break;
            case VMTYPE_DATA: typestr = "data"; break;
            case VMTYPE_ZERO: typestr = "data (zeroed)"; break;
            case VMTYPE_HEAP: typestr = "heap"; break;
            case VMTYPE_STACK: typestr = "stack"; break;
            case VMTYPE_KSTACK: typestr = "stack (kernel)"; break;
            case VMTYPE_SHARED: typestr = "shared memory"; break;
            case VMTYPE_FILE: typestr = "file"; break;
            case VMTYPE_MMAP: typestr = "mmap"; break;
            default: mos_warn("unknown memory region type %x", block.content);
        };

        pr_info("  %3zd: " PTR_FMT ", %5zd page(s), [%c%c%c%c%c%c, %c%c%c%c]: %s",
                i,                                                    //
                block.blk.vaddr,                                      //
                block.blk.npages,                                     //
                block.blk.flags & VM_READ ? 'r' : '-',                //
                block.blk.flags & VM_WRITE ? 'w' : '-',               //
                block.blk.flags & VM_EXEC ? 'x' : '-',                //
                block.blk.flags & VM_GLOBAL ? 'g' : '-',              //
                block.blk.flags & VM_USER ? 'u' : '-',                //
                block.blk.flags & VM_CACHE_DISABLED ? 'C' : '-',      //
                block.flags & VMBLOCK_FORK_PRIVATE ? 'p' : '-',       //
                block.flags & VMBLOCK_FORK_SHARED ? 's' : '-',        //
                block.flags & VMBLOCK_COW_COPY_ON_WRITE ? 'c' : '-',  //
                block.flags & VMBLOCK_COW_ZERO_ON_DEMAND ? 'z' : '-', //
                typestr                                               //
        );
    }
}
