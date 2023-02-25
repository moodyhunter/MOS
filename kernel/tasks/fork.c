// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "lib/structures/stack.h"
#include "mos/mm/cow.h"
#include "mos/mm/memops.h"
#include "mos/mm/paging/paging.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_types.h"
#include "mos/tasks/thread.h"

process_t *process_handle_fork(process_t *parent)
{
    MOS_ASSERT(process_is_valid(parent));

    process_t *child = process_allocate(parent, parent->name);
    if (unlikely(!child))
    {
        pr_emerg("failed to allocate process for fork");
        return NULL;
    }

    pr_emph("process %ld forked to %ld", parent->pid, child->pid);

    // copy the parent's memory
    for (int i = 0; i < parent->mmaps_count; i++)
    {
        proc_vmblock_t block = parent->mmaps[i];
        if (block.flags == VMBLOCK_FORK_SHARED)
        {
            pr_info2("fork %ld->%ld: shared " PTR_FMT "+%zu, flags = [%x]", parent->pid, child->pid, block.blk.vaddr, block.blk.npages, block.blk.flags);
            process_attach_mmap(child, block.blk, block.content, block.flags);
            continue;
        }

        vmblock_t child_vmblock;

        if (block.content == VMTYPE_KSTACK)
        {
            // Kernel stacks are special, we need to allocate a new one (not CoW-mapped)
            MOS_ASSERT_X(block.blk.npages == MOS_STACK_PAGES_KERNEL, "kernel stack size is not %d pages", MOS_STACK_PAGES_KERNEL);
            child_vmblock = mm_alloc_pages(child->pagetable, block.blk.npages, PGALLOC_HINT_STACK, block.blk.flags);
            pr_info2("fork %ld->%ld: kernel stack " PTR_FMT "+%zu, flags = [%x]", parent->pid, child->pid, block.blk.vaddr, block.blk.npages, block.blk.flags);
            process_attach_mmap(child, child_vmblock, VMTYPE_KSTACK, VMBLOCK_DEFAULT);
        }
        else
        {
            parent->mmaps[i].flags |= VMBLOCK_COW_COPY_ON_WRITE;
            mm_make_process_map_cow(parent->pagetable, block.blk.vaddr, child->pagetable, block.blk.vaddr, block.blk.npages);
            child_vmblock = parent->mmaps[i].blk; // do not use the return value from mm_make_process_map_cow
            pr_info2("fork %ld->%ld: CoW " PTR_FMT "+%zu, flags = [%x]", parent->pid, child->pid, block.blk.vaddr, block.blk.npages, block.blk.flags);
            process_attach_mmap(child, child_vmblock, block.content, VMBLOCK_COW_COPY_ON_WRITE);
        }
    }

    // copy the parent's files
    for (int i = 0; i < MOS_PROCESS_MAX_OPEN_FILES; i++)
    {
        io_t *file = parent->files[i];
        if (!file)
            continue; // skip empty slots
        if (file->closed)
            continue; // skip closed files
        child->files[i] = io_ref(file);
    }

    // copy the parent's threads
    for (int i = 0; i < parent->threads_count; i++)
    {
        thread_t *parent_thread = parent->threads[i];
        if (parent_thread != current_thread)
            continue;

        thread_t *child_thread = thread_allocate(child, parent_thread->mode);
        child_thread->u_stack = parent_thread->u_stack;
        child_thread->k_stack = parent_thread->k_stack;
        child_thread->name = strdup(parent_thread->name);
        child_thread->state = THREAD_STATE_CREATED;
        pr_info2("fork: thread %ld->%ld", parent_thread->tid, child_thread->tid);
        platform_setup_forked_context(parent_thread->context, &child_thread->context);

        process_attach_thread(child, child_thread);
        hashmap_put(thread_table, &child_thread->tid, child_thread);
    }

    hashmap_put(process_table, &child->pid, child);
    return child;
}
