// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.h"

#include <mos/mm/paging/paging.h>
#include <mos/mm/shm.h>
#include <mos/tasks/process.h>

// TODO: remove when file-based mmap is implemented

vmap_t *shm_allocate(size_t npages, vmap_fork_behavior_t type, vm_flags vmflags)
{
    process_t *owner = current_process;
    mos_debug(shm, "allocating %zu SHM pages in address space " PTR_FMT, npages, (ptr_t) owner->mm);
    vmap_t *vmap = mm_alloc_pages(owner->mm, npages, MOS_ADDR_USER_MMAP, VALLOC_DEFAULT, vmflags);
    if (!vmap)
    {
        pr_warn("failed to allocate shared memory block");
        return NULL;
    }

    vmap->content = VMAP_MMAP;
    vmap->fork_behavior = type;
    return vmap;
}

vmap_t *shm_map_shared_block(vmap_t *source, vmap_fork_behavior_t fork_behavior)
{
    process_t *owner = current_process;
    mos_debug(shm, "sharing %zu pages from address space " PTR_FMT " to address space " PTR_FMT, source->npages, (ptr_t) source->mmctx, (ptr_t) owner->mm);

    spinlock_acquire(&owner->mm->mm_lock);
    if (owner->mm != source->mmctx)
        spinlock_acquire(&source->mmctx->mm_lock);

    vmap_t *vmap = mm_clone_vmap_locked(source, owner->mm, NULL);

    if (owner->mm != source->mmctx)
        spinlock_release(&source->mmctx->mm_lock);
    spinlock_release(&owner->mm->mm_lock);

    vmap_finalise_init(vmap, VMAP_MMAP, fork_behavior);
    return vmap;
}
