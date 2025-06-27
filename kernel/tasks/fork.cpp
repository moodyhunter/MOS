// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/dentry.hpp"
#include "mos/filesystem/vfs.hpp"
#include "mos/mm/mm.hpp"
#include "mos/tasks/schedule.hpp"

#include <mos/lib/structures/hashmap.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/structures/stack.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/mm/cow.hpp>
#include <mos/mm/paging/paging.hpp>
#include <mos/mos_global.h>
#include <mos/platform/platform.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/process.hpp>
#include <mos/tasks/task_types.hpp>
#include <mos/tasks/thread.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

Process *process_do_fork(Process *parent)
{
    MOS_ASSERT(Process::IsValid(parent));

    auto child_p = Process::New(parent, parent->name);
    if (unlikely(!child_p))
    {
        mEmerg << "failed to allocate process for fork";
        return NULL;
    }

    child_p->working_directory = dentry_ref_up_to(parent->working_directory, root_dentry);

    dEmph<process> << "process " << parent->pid << " forked to " << child_p->pid;

    mm_lock_context_pair(parent->mm, child_p->mm);
    list_foreach(vmap_t, vmap_p, parent->mm->mmaps)
    {
        PtrResult<vmap_t> child_vmap = [&]()
        {
            switch (vmap_p->type)
            {
                case VMAP_TYPE_SHARED: return mm_clone_vmap_locked(vmap_p, child_p->mm); break;
                case VMAP_TYPE_PRIVATE: return cow_clone_vmap_locked(child_p->mm, vmap_p); break;
                default: mos_panic("unknown vmap"); break;
            }
        }();

        if (child_vmap.isErr())
        {
            mos_panic("failed to clone vmap");
        }

        dInfo2<process> << "fork vmap " << parent->pid << "->" << child_p->pid << ": " << get_vmap_type_str(vmap_p->type) << ", " << (void *) vmap_p << " -> "
                        << (void *) child_vmap.get();
        vmap_finalise_init(child_vmap.get(), vmap_p->content, vmap_p->type);
    }

    mm_unlock_context_pair(parent->mm, child_p->mm);

    // copy the parent's files
    for (int i = 0; i < MOS_PROCESS_MAX_OPEN_FILES; i++)
    {
        const auto &file = parent->files[i];
        if (IO::IsValid(file.io))
        {
            child_p->files[i].io = file.io->ref();
            child_p->files[i].flags = file.flags;
        }
    }

    for (int i = 0; i < SIGNAL_MAX_N; i++)
        child_p->signal_info.handlers[i] = parent->signal_info.handlers[i];
    child_p->signal_info.sigchild_waitlist.reset();

    // copy the thread
    const auto parent_thread = current_thread;
    const auto child_t = thread_allocate(child_p, parent_thread->mode);
    dInfo2<process> << "fork: thread " << parent_thread->tid << "->" << child_t->tid;
    child_t->u_stack = parent_thread->u_stack;
    child_t->name = parent_thread->name;
    const ptr_t kstack_blk = phyframe_va(mm_get_free_pages(MOS_STACK_PAGES_KERNEL));
    stack_init(&child_t->k_stack, (void *) kstack_blk, MOS_STACK_PAGES_KERNEL * MOS_PAGE_SIZE);
    spinlock_acquire(&parent_thread->signal_info.lock);
    child_t->signal_info.mask = parent_thread->signal_info.mask;
    for (const auto sig : parent_thread->signal_info.pending)
    {
        child_t->signal_info.pending.push_back(sig);
    }
    spinlock_release(&parent_thread->signal_info.lock);

    platform_context_clone(parent_thread, child_t);

    ProcessTable.insert(child_p->pid, child_p);
    thread_complete_init(child_t);
    scheduler_add_thread(child_t);
    return child_p;
}
