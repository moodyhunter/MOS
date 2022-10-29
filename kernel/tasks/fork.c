// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/structures/hashmap.h"
#include "lib/structures/stack.h"
#include "mos/mm/cow.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/tasks/task_type.h"
#include "mos/tasks/thread.h"

process_t *process_handle_fork(process_t *parent)
{
    MOS_ASSERT(process_is_valid(parent));
    pr_info("process %d forked", parent->pid);

    process_dump_mmaps(parent);

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

        parent->mmaps[i].map_flags |= MMAP_COW;
        vmblock_t child_vmblock = mm_make_process_map_cow(parent->pagetable, block.vm.vaddr, child->pagetable, block.vm.vaddr, block.vm.npages);

        process_attach_mmap(child, child_vmblock, block.type, true);
    }

    // copy the parent's files
    for (int i = 0; i < parent->files_count; i++)
    {
        io_t *file = parent->files[i];
        io_ref(file); // increase the refcount
        process_attach_fd(child, file);
    }

    // copy the parent's threads
    for (int i = 0; i < parent->threads_count; i++)
    {
        thread_t *parent_thread = parent->threads[i];
        if (parent_thread->status == THREAD_STATUS_DEAD)
            continue;
        thread_t *child_thread = thread_allocate(child, parent_thread->flags);
        child_thread->stack = parent_thread->stack;
        child_thread->status = parent_thread->status;
        child_thread->current_instruction = parent_thread->current_instruction;

        switch (child_thread->status)
        {
            case THREAD_STATUS_READY:
            case THREAD_STATUS_WAITING:
            {
                child_thread->status = THREAD_STATUS_FORKED;
                break;
            }

            case THREAD_STATUS_RUNNING:
            {
                mos_panic("don't know how to handle running threads");
            }

            case THREAD_STATUS_CREATED: // keep the thread in the created state
            case THREAD_STATUS_FORKED:  // keep the thread in the forked state
            case THREAD_STATUS_DYING:
            case THREAD_STATUS_DEAD:
            {
                break;
            }
        }

        if (parent->main_thread == parent_thread)
            child->main_thread = child_thread;

        process_attach_thread(child, child_thread);
        hashmap_put(thread_table, &child_thread->tid, child_thread);
    }

    process_dump_mmaps(child);
    hashmap_put(process_table, &child->pid, child);
    return child;
}
