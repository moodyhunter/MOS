// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/hashmap.h"
#include "lib/structures/stack.h"
#include "mos/mm/cow.h"
#include "mos/mm/memops.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_type.h"
#include "mos/tasks/thread.h"

process_t *process_handle_fork(process_t *parent)
{
    MOS_ASSERT(process_is_valid(parent));
    mos_debug("process %d forked", parent->pid);

    process_t *child = process_allocate(parent, parent->effective_uid, parent->name);

    // copy the parent's memory
    for (int i = 0; i < parent->mmaps_count; i++)
    {
        proc_vmblock_t block = parent->mmaps[i];
        if (block.map_flags & MMAP_PRIVATE)
        {
            mos_debug("private mapping, skipping");
            continue;
        }

        vmblock_t child_vmblock;
        if (block.type == VMTYPE_KSTACK)
        {
            // Kernel stacks are special, we need to allocate a new one (not CoW-mapped)
            MOS_ASSERT_X(block.vm.npages == MOS_STACK_PAGES_KERNEL, "kernel stack size is not %d pages", MOS_STACK_PAGES_KERNEL);
            child_vmblock = platform_mm_alloc_pages_at(child->pagetable, block.vm.vaddr, block.vm.npages, block.vm.flags);
            process_attach_mmap(child, child_vmblock, VMTYPE_KSTACK, MMAP_DEFAULT);
        }
        else
        {
            parent->mmaps[i].map_flags |= MMAP_COW;
            child_vmblock = mm_make_process_map_cow(parent->pagetable, block.vm.vaddr, child->pagetable, block.vm.vaddr, block.vm.npages);
            process_attach_mmap(child, child_vmblock, block.type, MMAP_COW);
        }
    }

    // copy the parent's files
    for (int i = 0; i < parent->files_count; i++)
    {
        io_t *file = parent->files[i];
        process_attach_ref_fd(child, file);
    }

    // copy the parent's threads
    for (int i = 0; i < parent->threads_count; i++)
    {
        thread_t *parent_thread = parent->threads[i];
        if (parent_thread != current_thread)
            continue;

        thread_t *child_thread = thread_allocate(child, parent_thread->flags);
        child_thread->stack = parent_thread->stack;
        child_thread->kernel_stack = parent_thread->kernel_stack;
        child_thread->status = THREAD_STATUS_CREATED;
        platform_context_copy(parent_thread->context, &child_thread->context);

        process_attach_thread(child, child_thread);
        hashmap_put(thread_table, &child_thread->tid, child_thread);
    }

    hashmap_put(process_table, &child->pid, child);
    return child;
}
