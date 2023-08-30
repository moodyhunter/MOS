// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/io/io.h"
#include "mos/mm/mm.h"
#include "mos/platform/platform.h"

#include <mos/mm/cow.h>
#include <mos/mm/mm_types.h>
#include <mos/mm/mmap.h>
#include <mos/mm/paging/paging.h>
#include <mos/mos_global.h>
#include <mos/tasks/process.h>
#include <mos/tasks/task_types.h>

/**
 * @brief Check if the mmap flags are valid
 *
 * @param hint_addr The hint address
 * @param mmap_flags The mmap flags
 */
static bool mmap_verify_arguments(ptr_t *hint_addr, mmap_flags_t mmap_flags)
{
    if ((*hint_addr % MOS_PAGE_SIZE) != 0)
    {
        pr_warn("hint address must be page-aligned");
        return false;
    }
    const bool shared = mmap_flags & MMAP_SHARED;   // when forked, shared between parent and child
    const bool private = mmap_flags & MMAP_PRIVATE; // when forked, make it Copy-On-Write

    if (shared == private)
    {
        pr_warn("mmap_file: shared and private are mutually exclusive, and one of them must be specified");
        return NULL;
    }

    if ((*hint_addr == 0) && (mmap_flags & MMAP_EXACT))
    {
        // WTF is this? Trying to map at address 0?
        pr_warn("mmap_anonymous: trying to map at address 0");
        return false;
    }

    if (*hint_addr == 0)
        *hint_addr = MOS_ADDR_USER_MMAP;

    return true;
}

ptr_t mmap_anonymous(mm_context_t *ctx, ptr_t hint_addr, mmap_flags_t flags, vm_flags vm_flags, size_t n_pages)
{
    if (!mmap_verify_arguments(&hint_addr, flags))
        return 0;

    const valloc_flags valloc_flags = (flags & MMAP_EXACT) ? VALLOC_EXACT : VALLOC_DEFAULT;

    vmap_t *vmap = cow_allocate_zeroed_pages(ctx, n_pages, hint_addr, valloc_flags, vm_flags);
    mos_debug(mmap, "allocated %zd pages at " PTR_FMT, vmap->npages, vmap->vaddr);

    const vmap_fork_behavior_t type = (flags & MMAP_SHARED) ? VMAP_FORK_SHARED : VMAP_FORK_PRIVATE;
    vmap_finalise_init(vmap, VMAP_MMAP, type);
    return vmap->vaddr;
}

ptr_t mmap_file(mm_context_t *ctx, ptr_t hint_addr, mmap_flags_t flags, vm_flags vm_flags, size_t n_pages, io_t *io, off_t offset)
{
    if (!mmap_verify_arguments(&hint_addr, flags))
        return 0;

    if (offset % MOS_PAGE_SIZE != 0)
    {
        pr_warn("mmap_file: offset must be page-aligned");
        return 0;
    }

    const valloc_flags valloc_flags = (flags & MMAP_EXACT) ? VALLOC_EXACT : VALLOC_DEFAULT;
    const vmap_fork_behavior_t type = (flags & MMAP_SHARED) ? VMAP_FORK_SHARED : VMAP_FORK_PRIVATE;

    mm_lock_ctx_pair(ctx, NULL);
    vmap_t *vmap = mm_get_free_vaddr_locked(ctx, n_pages, hint_addr, valloc_flags);
    mm_unlock_ctx_pair(ctx, NULL);

    if (unlikely(!vmap))
    {
        pr_warn("mmap_file: no free virtual address space");
        return 0;
    }

    vmap->vmflags = vm_flags;
    vmap->fork_behavior = type;

    if (!io_mmap(io, vmap, offset))
    {
        vmap_destroy(vmap);
        pr_warn("mmap_file: could not map the file");
        return 0;
    }

    vmap_finalise_init(vmap, VMAP_FILE, type);
    return vmap->vaddr;
}

bool munmap(ptr_t addr, size_t size)
{
    // will unmap all pages containing the range, even if they are not fully contained
    const ptr_t range_start = ALIGN_DOWN_TO_PAGE(addr);
    const ptr_t range_end = ALIGN_UP_TO_PAGE(addr + size);
    const size_t n_pages = (range_end - range_start) / MOS_PAGE_SIZE;

    mm_unmap_pages(current_process->mm, range_start, n_pages);

    return true;
}
