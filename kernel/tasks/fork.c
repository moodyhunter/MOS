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

extern const char *vmap_type_str[];

process_t *process_handle_fork(process_t *parent)
{
    MOS_ASSERT(process_is_valid(parent));

    process_t *child_p = process_allocate(parent, parent->name);
    if (unlikely(!child_p))
    {
        pr_emerg("failed to allocate process for fork");
        return NULL;
    }

#if MOS_DEBUG_FEATURE(fork)
    pr_emph("process %d forked to %d", parent->pid, child_p->pid);
#endif

    mm_lock_ctx_pair(parent->mm, child_p->mm);
    list_foreach(vmap_t, vmap_p, parent->mm->mmaps)
    {
        vmap_t *child_vmap = NULL;
        switch (vmap_p->type)
        {
            case VMAP_TYPE_SHARED: child_vmap = mm_clone_vmap_locked(vmap_p, child_p->mm); break;
            case VMAP_TYPE_PRIVATE: child_vmap = cow_clone_vmap_locked(child_p->mm, vmap_p); break;
            default: mos_panic("unknown vmap"); break;
        }
#if MOS_DEBUG_FEATURE(fork)
        pr_info2("fork %d->%d: %10s, parent vmap: %pvm, child vmap: %pvm", parent->pid, child_p->pid, vmap_type_str[vmap_p->type], (void *) vmap_p, (void *) child_vmap);
#endif
        vmap_finalise_init(child_vmap, vmap_p->content, vmap_p->type);
    }

    mm_unlock_ctx_pair(parent->mm, child_p->mm);

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
    child_t->name = strdup(parent_thread->name);
    const ptr_t kstack_blk = phyframe_va(mm_get_free_pages(MOS_STACK_PAGES_KERNEL));
    stack_init(&child_t->k_stack, (void *) kstack_blk, MOS_STACK_PAGES_KERNEL * MOS_PAGE_SIZE);
#if MOS_DEBUG_FEATURE(fork)
    pr_info2("fork: thread %d->%d", parent_thread->tid, child_t->tid);
#endif
    platform_setup_forked_context(parent_thread->context, &child_t->context);

    hashmap_put(&thread_table, child_t->tid, child_t);
    hashmap_put(&process_table, child_p->pid, child_p);
    thread_setup_complete(child_t, NULL, NULL);
    return child_p;
}
