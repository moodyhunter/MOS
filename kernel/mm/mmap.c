// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/io/io.h"
#include "mos/mm/mm.h"
#include "mos/mm/paging/table_ops.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

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

    if (mmap_flags & MMAP_EXACT)
    {
        // always use the hint address if MMAP_EXACT is specified
        return true;
    }
    else
    {
        // if no hint address is specified, use the default
        if (*hint_addr == 0)
            *hint_addr = MOS_ADDR_USER_MMAP;
    }

    return true;
}

ptr_t mmap_anonymous(mm_context_t *ctx, ptr_t hint_addr, mmap_flags_t flags, vm_flags vm_flags, size_t n_pages)
{
    if (!mmap_verify_arguments(&hint_addr, flags))
        return 0;

    const valloc_flags valloc_flags = (flags & MMAP_EXACT) ? VALLOC_EXACT : VALLOC_DEFAULT;

    vmap_t *vmap = cow_allocate_zeroed_pages(ctx, n_pages, hint_addr, valloc_flags, vm_flags);
    pr_dinfo2(mmap, "allocated %zd pages at " PTR_FMT, vmap->npages, vmap->vaddr);

    const vmap_type_t type = (flags & MMAP_SHARED) ? VMAP_TYPE_SHARED : VMAP_TYPE_PRIVATE;
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
    const vmap_type_t type = (flags & MMAP_SHARED) ? VMAP_TYPE_SHARED : VMAP_TYPE_PRIVATE;

    mm_lock_ctx_pair(ctx, NULL);
    vmap_t *vmap = mm_get_free_vaddr_locked(ctx, n_pages, hint_addr, valloc_flags);
    mm_unlock_ctx_pair(ctx, NULL);

    if (unlikely(!vmap))
    {
        pr_warn("mmap_file: no free virtual address space");
        return 0;
    }

    vmap->vmflags = vm_flags;
    vmap->type = type;

    if (!io_mmap(io, vmap, offset))
    {
        vmap_destroy(vmap);
        pr_warn("mmap_file: could not map the file: io_mmap() failed");
        return 0;
    }

    vmap_finalise_init(vmap, VMAP_FILE, type);
    return vmap->vaddr;
}

bool munmap(ptr_t addr, size_t size)
{
    spinlock_acquire(&current_process->mm->mm_lock);
    vmap_t *const whole_map = vmap_obtain(current_process->mm, addr, NULL);
    if (unlikely(!whole_map))
    {
        spinlock_release(&current_process->mm->mm_lock);
        pr_warn("munmap: could not find the vmap");
        return false;
    }

    // will unmap all pages containing the range, even if they are not fully contained
    const ptr_t range_start = ALIGN_DOWN_TO_PAGE(addr);
    const ptr_t range_end = ALIGN_UP_TO_PAGE(addr + size);

    const size_t start_pgoff = (range_start - whole_map->vaddr) / MOS_PAGE_SIZE;
    const size_t end_pgoff = (range_end - whole_map->vaddr) / MOS_PAGE_SIZE;

    vmap_t *const range_map = vmap_split_for_range(whole_map, start_pgoff, end_pgoff);
    if (unlikely(!range_map))
    {
        pr_warn("munmap: could not split the vmap");
        spinlock_release(&current_process->mm->mm_lock);
        spinlock_release(&whole_map->lock);
        return false;
    }

    vmap_destroy(range_map);
    spinlock_release(&current_process->mm->mm_lock);
    spinlock_release(&whole_map->lock);
    return true;
}

bool vm_protect(mm_context_t *mmctx, ptr_t addr, size_t size, vm_flags perm)
{
    MOS_ASSERT(addr % MOS_PAGE_SIZE == 0);
    size = ALIGN_UP_TO_PAGE(size);

    spinlock_acquire(&mmctx->mm_lock);
    vmap_t *const first_part = vmap_obtain(mmctx, addr, NULL);
    const size_t addr_pgoff = (addr - first_part->vaddr) / MOS_PAGE_SIZE;

    //
    // first | second | third
    //       ^        ^
    //       |        |
    //       addr     addr + size
    //

    vmap_t *const to_protect = __extension__({
        vmap_t *vmap = first_part;
        // if 'addr_pgoff' is 0, then the first part is the one we want to protect,
        // otherwise we need to split it to get the vmap that starts at 'addr'
        if (addr_pgoff)
        {
            vmap = vmap_split(first_part, addr_pgoff);
            spinlock_release(&first_part->lock); // release the lock on the first part, we don't need it
        }

        vmap;
    });

    const size_t size_pgoff = size / MOS_PAGE_SIZE;
    if (size_pgoff < to_protect->npages)
    {
        // if there is a third part
        vmap_t *const part3 = vmap_split(to_protect, size_pgoff);
        spinlock_release(&part3->lock); // release the lock on the third part, we don't need it
    }

    if (to_protect->io)
    {
        if (!io_mmap_perm_check(to_protect->io, perm, to_protect->type == VMAP_TYPE_PRIVATE))
        {
            spinlock_release(&to_protect->lock); // permission denied
            spinlock_release(&mmctx->mm_lock);
            return false;
        }
    }

    const bool read_lost = to_protect->vmflags & VM_READ && !(perm & VM_READ);    // if we lose read permission
    const bool write_lost = to_protect->vmflags & VM_WRITE && !(perm & VM_WRITE); // if we lose write permission
    const bool exec_lost = to_protect->vmflags & VM_EXEC && !(perm & VM_EXEC);    // if we lose exec permission

    vm_flags mask = 0;
    if (read_lost)
    {
        mask |= VM_READ;
        pr_warn("read permission lost, this is not supported yet");
    }

    if (write_lost)
        mask |= VM_WRITE;
    if (exec_lost)
        mask |= VM_EXEC;

    // remove permissions immediately
    mm_do_mask_flags(mmctx->pgd, to_protect->vaddr, to_protect->npages, mask);

    // do not add permissions immediately, we will let the page fault handler do it
    // e.g. write permission granted only when the page is written to (and proper e.g. CoW)
    // can be done.

    // let page fault handler do the real flags update
    to_protect->vmflags = perm | VM_USER;

    spinlock_release(&to_protect->lock);
    spinlock_release(&mmctx->mm_lock);
    return true;
}
