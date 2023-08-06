// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.h"

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

#define FORKFMT "fork %d->%d: %10s " PTR_FMT "+%-3zu flags [0x%x]"

static const char *fork_behavior_string[] = {
    [VMAP_FORK_INVALID] = "invalid",
    [VMAP_FORK_ZEROED] = "zeroed",
    [VMAP_FORK_SHARED] = "shared",
    [VMAP_FORK_PRIVATE] = "private",
};

process_t *process_handle_fork(process_t *parent)
{
    MOS_ASSERT(process_is_valid(parent));

    process_t *child_p = process_allocate(parent, parent->name);
    if (unlikely(!child_p))
    {
        pr_emerg("failed to allocate process for fork");
        return NULL;
    }

    pr_emph("process %d forked to %d", parent->pid, child_p->pid);

    // copy the parent's memory

    list_foreach(vmap_t, vmap_p, parent->mm->mmaps)
    {
        pr_info2(FORKFMT, parent->pid, child_p->pid, fork_behavior_string[vmap_p->fork_behavior], vmap_p->vaddr, vmap_p->npages, vmap_p->vmflags);
        switch (vmap_p->fork_behavior)
        {
            case VMAP_FORK_ZEROED:
            {
                // Kernel stacks are special, we need to allocate a new one (not CoW-mapped)
                MOS_ASSERT(vmap_p->content == VMAP_KSTACK);
                MOS_ASSERT_X(vmap_p->npages == MOS_STACK_PAGES_KERNEL, "kernel stack size is not %d pages", MOS_STACK_PAGES_KERNEL);
                vmap_t *child_vmap = mm_alloc_pages(child_p->mm, vmap_p->npages, MOS_ADDR_USER_STACK, VALLOC_DEFAULT, vmap_p->vmflags);
                vmap_finalise_init(child_vmap, vmap_p->content, vmap_p->fork_behavior);
                break;
            }
            case VMAP_FORK_SHARED:
            {
                vmap_t *child_vmap = mm_clone_vmap(vmap_p, child_p->mm, NULL);
                vmap_finalise_init(child_vmap, vmap_p->content, vmap_p->fork_behavior);
                break;
            }
            case VMAP_FORK_PRIVATE:
            {
                vmap_t *child_vmap = cow_clone_vmap(child_p->mm, vmap_p);
                vmap_finalise_init(child_vmap, vmap_p->content, vmap_p->fork_behavior);
                break;
            }

            case VMAP_FORK_INVALID: mos_warn("unknown vmap"); break;
        }
    }

    // copy the parent's files
    for (int i = 0; i < MOS_PROCESS_MAX_OPEN_FILES; i++)
    {
        io_t *file = parent->files[i];
        if (io_valid(file))
            child_p->files[i] = io_ref(file);
    }

    // copy the thread
    const thread_t *parent_thread = current_thread;
    thread_t *child_t = thread_allocate(child_p, parent_thread->mode);
    child_t->u_stack = parent_thread->u_stack;
    child_t->k_stack = parent_thread->k_stack;
    child_t->name = strdup(parent_thread->name);
    pr_info2("fork: thread %d->%d", parent_thread->tid, child_t->tid);
    platform_setup_forked_context(parent_thread->context, &child_t->context);
    process_attach_thread(child_p, child_t);

    hashmap_put(&thread_table, child_t->tid, child_t);
    hashmap_put(&process_table, child_p->pid, child_p);
    thread_setup_complete(child_t);
    return child_p;
}
