// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/process.h"

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "lib/structures/hashmap_common.h"
#include "lib/sync/spinlock.h"
#include "mos/io/terminal.h"
#include "mos/mm/cow.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/memops.h"
#include "mos/mm/paging/paging.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/thread.h"

#define PROCESS_HASHTABLE_SIZE 512

hashmap_t *process_table = { 0 }; // pid_t -> process_t

static hash_t process_hash(const void *key)
{
    return (hash_t){ .hash = *(pid_t *) key };
}

static pid_t new_process_id(void)
{
    static pid_t next = 1;
    return (pid_t){ next++ };
}

#if MOS_DEBUG_FEATURE(process)
static void debug_dump_process(void)
{
    if (current_thread)
    {
        process_t *proc = current_process;
        printk("process %d (%s) ", proc->pid, proc->name);
        if (proc->parent)
            printk("parent %d (%s) ", proc->parent->pid, proc->parent->name);
        else
            printk("parent <none> ");
        printk("uid %d ", proc->effective_uid);
        process_dump_mmaps(proc);
    }
}
#endif

process_t *process_allocate(process_t *parent, uid_t euid, const char *name)
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
        pr_emph("special process %d (%s) created", proc->pid, name);
    }
    else
    {
        pr_emerg("process %d has no parent", proc->pid);
        kfree(proc);
        return NULL;
    }

    proc->name = strdup(name ? name : "<unknown>");
    proc->effective_uid = euid;

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
        pr_emerg("failed to create page table for process %d (%s)", proc->pid, proc->name);
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
#if MOS_DEBUG_FEATURE(process)
    declare_panic_hook(debug_dump_process);
    install_panic_hook(&debug_dump_process_holder);
#endif
}

void process_deinit(void)
{
    hashmap_deinit(process_table);
    kfree(process_table);
}

process_t *process_new(process_t *parent, uid_t euid, const char *name, terminal_t *term, thread_entry_t entry, argv_t argv)
{
    process_t *proc = process_allocate(parent, euid, name);
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
    process_attach_mmap(proc, heap, VMTYPE_HEAP, MMAP_DEFAULT);

    void *old_proc = hashmap_put(process_table, &proc->pid, proc);
    MOS_ASSERT_X(old_proc == NULL, "process already exists, go and buy yourself a lottery :)");
    return proc;
}

process_t *process_get(pid_t pid)
{
    process_t *p = hashmap_get(process_table, &pid);
    if (p == NULL)
        mos_warn("process %d not found", pid);
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
            mos_warn("process %d has too many open files", process->pid);
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
    mos_debug(process, "process %d attached thread %d", process->pid, thread->tid);
    process->threads[process->threads_count++] = thread;
}

void process_attach_mmap(process_t *process, vmblock_t block, vm_type type, mmap_flags flags)
{
    MOS_ASSERT(process_is_valid(process));
    process->mmaps = krealloc(process->mmaps, sizeof(proc_vmblock_t) * (process->mmaps_count + 1));
    process->mmaps[process->mmaps_count++] = (proc_vmblock_t){ .vm = block, .type = type, .map_flags = flags };
}

void process_handle_exit(process_t *process, int exit_code)
{
    MOS_ASSERT(process_is_valid(process));
    pr_info("process %d exited with code %d", process->pid, exit_code);

    mos_debug(process, "terminating all %lu threads owned by %d", process->threads_count, process->pid);
    for (int i = 0; i < process->threads_count; i++)
    {
        // TODO: if a thread is being executed, we should wait for it to finish
        // TODO: if a thread holds a lock, we should release it?
        thread_t *thread = process->threads[i];
        spinlock_acquire(&thread->state_lock);
        if (thread->state == THREAD_STATE_DEAD)
        {
            pr_warn("thread %d is already dead", thread->tid);
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

    mos_debug(process, "closed %zu/%zu files owned by %d", files_already_closed, files_total, process->pid);
}

void process_handle_cleanup(process_t *process)
{
    MOS_ASSERT(process_is_valid(process));
    MOS_ASSERT_X(current_process != process, "cannot cleanup current process");

    mos_debug(process, "unmapping all %lu memory regions owned by %d", process->mmaps_count, process->pid);
    for (int i = 0; i < process->mmaps_count; i++)
    {
        const mmap_flags flags = process->mmaps[i].map_flags;
        const vmblock_t block = process->mmaps[i].vm;

        // they will be unmapped when the last process detaches them
        // !! TODO: How to track this??
        if (flags & MMAP_COW)
            continue;

        mm_free_pages(process->pagetable, block);
    }
}

uintptr_t process_grow_heap(process_t *process, size_t npages)
{
    MOS_ASSERT(process_is_valid(process));

    proc_vmblock_t *heap = NULL;
    for (int i = 0; i < process->mmaps_count; i++)
    {
        if (process->mmaps[i].type == VMTYPE_HEAP)
        {
            heap = &process->mmaps[i];
            spinlock_acquire(&heap->lock);
            break;
        }
    }

    MOS_ASSERT(heap != NULL);

    const uintptr_t heap_top = heap->vm.vaddr + heap->vm.npages * MOS_PAGE_SIZE;

    if (heap->map_flags & MMAP_COW)
    {
        vmblock_t zeroed = mm_alloc_zeroed_pages_at(process->pagetable, heap_top, npages, VM_USER_RW);
        MOS_ASSERT(zeroed.npages == npages);
    }
    else
    {
        vmblock_t new_part = mm_alloc_pages_at(process->pagetable, heap_top, npages, VM_USER_RW);
        if (new_part.vaddr == 0 || new_part.npages != npages)
        {
            mos_warn("failed to grow heap of process %d", process->pid);
            mm_free_pages(process->pagetable, new_part);
            spinlock_release(&heap->lock);
            return heap_top;
        }
    }

    pr_info2("grew heap of process %d by %zu pages", process->pid, npages);
    heap->vm.npages += npages;
    spinlock_release(&heap->lock);
    return heap_top + npages * MOS_PAGE_SIZE;
}

void process_dump_mmaps(process_t *process)
{
    pr_info("process %d has %lu memory regions:", process->pid, process->mmaps_count);
    for (int i = 0; i < process->mmaps_count; i++)
    {
        proc_vmblock_t block = process->mmaps[i];
        const char *type = "unknown";
        switch (block.type)
        {
            case VMTYPE_APPCODE: type = "code"; break;
            case VMTYPE_APPDATA: type = "data"; break;
            case VMTYPE_APPDATA_ZERO: type = "data (zeroed)"; break;
            case VMTYPE_HEAP: type = "heap"; break;
            case VMTYPE_STACK: type = "stack"; break;
            case VMTYPE_KSTACK: type = "stack (kernel)"; break;
            case VMTYPE_SHM: type = "shared memory"; break;
            case VMTYPE_FILE: type = "file"; break;
            default: MOS_UNREACHABLE();
        };

        pr_info("  block %d: " PTR_FMT ", % 3zd page(s), [%c%c%c%c%c%c%c]: %s",
                i,                                          //
                block.vm.vaddr,                             //
                block.vm.npages,                            //
                block.vm.flags & VM_READ ? 'r' : '-',       //
                block.vm.flags & VM_WRITE ? 'w' : '-',      //
                block.vm.flags & VM_EXEC ? 'x' : '-',       //
                block.vm.flags & VM_GLOBAL ? 'g' : '-',     //
                block.vm.flags & VM_USER ? 'u' : '-',       //
                block.map_flags & MMAP_COW ? 'c' : '-',     //
                block.map_flags & MMAP_PRIVATE ? 'p' : '-', //
                type                                        //
        );
    }
}
