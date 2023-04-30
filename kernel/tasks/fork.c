// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/lib/structures/hashmap.h>
#include <mos/lib/structures/stack.h>
#include <mos/mm/cow.h>
#include <mos/mm/paging/paging.h>
#include <mos/mos_global.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/tasks/process.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/thread.h>
#include <string.h>

#define FORKFMT "fork %ld->%ld: %10s " PTR_FMT "+%-3zu flags [0x%x]"

process_t *process_handle_fork(process_t *parent)
{
    MOS_ASSERT(process_is_valid(parent));

    process_t *child_p = process_allocate(parent, parent->name);
    if (unlikely(!child_p))
    {
        pr_emerg("failed to allocate process for fork");
        return NULL;
    }

    pr_emph("process %ld forked to %ld", parent->pid, child_p->pid);

    // copy the parent's memory
    for (size_t i = 0; i < parent->mmaps_count; i++)
    {
        vmap_t *vmap = &parent->mmaps[i];
        switch (vmap->content)
        {
            case VMTYPE_KSTACK:
            {
                // Kernel stacks are special, we need to allocate a new one (not CoW-mapped)
                MOS_ASSERT_X(vmap->blk.npages == MOS_STACK_PAGES_KERNEL, "kernel stack size is not %d pages", MOS_STACK_PAGES_KERNEL);
                const vmblock_t block = mm_alloc_pages(child_p->pagetable, vmap->blk.npages, MOS_ADDR_USER_STACK, VALLOC_DEFAULT, vmap->blk.flags);

                pr_info2(FORKFMT, parent->pid, child_p->pid, "kstack", vmap->blk.vaddr, vmap->blk.npages, vmap->blk.flags);
                process_attach_mmap(child_p, block, VMTYPE_KSTACK, (vmap_flags_t){ 0 });
                break;
            }

            case VMTYPE_FILE:
            case VMTYPE_MMAP:
            {
                MOS_ASSERT_X(vmap->flags.fork_mode != VMAP_FORK_NA, "invalid fork mode for mmap");
                if (vmap->flags.fork_mode == VMAP_FORK_SHARED)
                {
                    // TODO: shared mmap, just process_attach_mmap() if
                    break;
                }

                [[fallthrough]]; // private mmap, fall through to CoW
            }
            case VMTYPE_CODE:
            case VMTYPE_DATA:
            case VMTYPE_HEAP:
            case VMTYPE_STACK:
            {
                vmap->flags.cow = true;
                const vmblock_t block = { 0 };

                // TODO: make the block as CoW using mm_make_cow_block()

                pr_info2(FORKFMT, parent->pid, child_p->pid, "CoW", vmap->blk.vaddr, vmap->blk.npages, vmap->blk.flags);
                process_attach_mmap(child_p, block, vmap->content, vmap->flags);
                break;
            }
        }
    }

    // copy the parent's files
    for (int i = 0; i < MOS_PROCESS_MAX_OPEN_FILES; i++)
    {
        // check if the io_t is valid using io_valid()
        // if it is valid, then we io_ref() it and put it in the child's file table
        // AT THE SAME OFFSET
    }

    // copy the thread
    const thread_t *parent_thread = current_thread;
    thread_t *child_t = thread_allocate(child_p, parent_thread->mode);
    child_t->u_stack = parent_thread->u_stack;
    child_t->k_stack = parent_thread->k_stack;
    child_t->name = strdup(parent_thread->name);
    pr_info2("fork: thread %ld->%ld", parent_thread->tid, child_t->tid);
    platform_setup_forked_context(parent_thread->context, &child_t->context);
    process_attach_thread(child_p, child_t);

    hashmap_put(thread_table, child_t->tid, child_t);
    hashmap_put(process_table, child_p->pid, child_p);
    thread_setup_complete(child_t);
    return child_p;
}
