// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/dentry.h"
#include "mos/filesystem/vfs.h"
#include "mos/mm/mm.h"
#include "mos/tasks/signal.h"

#include <mos/lib/structures/hashmap.h>
#include <mos/lib/structures/list.h>
#include <mos/lib/structures/stack.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/cow.h>
#include <mos/mm/paging/paging.h>
#include <mos/mos_global.h>
#include <mos/platform/platform.h>
#include <mos/syslog/printk.h>
#include <mos/tasks/process.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/thread.h>
#include <mos_stdlib.h>
#include <mos_string.h>

extern const char *vmap_type_str[];

process_t *process_do_fork(process_t *parent)
{
    MOS_ASSERT(process_is_valid(parent));

    process_t *child_p = process_allocate(parent, parent->name);
    if (unlikely(!child_p))
    {
        pr_emerg("failed to allocate process for fork");
        return NULL;
    }

    child_p->working_directory = dentry_ref_up_to(parent->working_directory, root_dentry);

#if MOS_DEBUG_FEATURE(process)
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
#if MOS_DEBUG_FEATURE(process)
        pr_info2("fork %d->%d: %10s, parent vmap: %pvm, child vmap: %pvm", parent->pid, child_p->pid, vmap_type_str[vmap_p->type], (void *) vmap_p, (void *) child_vmap);
#endif
        vmap_finalise_init(child_vmap, vmap_p->content, vmap_p->type);
    }

    mm_unlock_ctx_pair(parent->mm, child_p->mm);

    // copy the parent's files
    for (int i = 0; i < MOS_PROCESS_MAX_OPEN_FILES; i++)
    {
        fd_type file = parent->files[i];
        if (io_valid(file.io))
        {
            child_p->files[i].io = io_ref(file.io);
            child_p->files[i].flags = file.flags;
        }
    }

    child_p->signal_info = parent->signal_info;
    waitlist_init(&child_p->signal_info.sigchild_waitlist);

    // copy the thread
    thread_t *const parent_thread = current_thread;
    thread_t *child_t = thread_allocate(child_p, parent_thread->mode);
    child_t->u_stack = parent_thread->u_stack;
    child_t->name = strdup(parent_thread->name);
    const ptr_t kstack_blk = phyframe_va(mm_get_free_pages(MOS_STACK_PAGES_KERNEL));
    stack_init(&child_t->k_stack, (void *) kstack_blk, MOS_STACK_PAGES_KERNEL * MOS_PAGE_SIZE);
#if MOS_DEBUG_FEATURE(process)
    pr_info2("fork: thread %d->%d", parent_thread->tid, child_t->tid);
#endif
    spinlock_acquire(&parent_thread->signal_info.lock);
    memcpy(child_t->signal_info.masks, parent_thread->signal_info.masks, sizeof(bool) * SIGNAL_MAX_N);
    list_foreach(sigpending_t, sig, parent_thread->signal_info.pending)
    {
        sigpending_t *new_sig = kmalloc(sigpending_slab);
        linked_list_init(list_node(new_sig));
        new_sig->signal = sig->signal;
        list_node_prepend(&child_t->signal_info.pending, list_node(new_sig));
    }
    spinlock_release(&parent_thread->signal_info.lock);

    platform_context_clone(parent_thread, child_t);

    hashmap_put(&process_table, child_p->pid, child_p);
    thread_complete_init(child_t);
    return child_p;
}
