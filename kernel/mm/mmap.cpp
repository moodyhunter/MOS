// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/io/io.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/paging/table_ops.hpp"
#include "mos/platform/platform.hpp"
#include "mos/syslog/printk.hpp"

#include <mos/mm/cow.hpp>
#include <mos/mm/mm_types.h>
#include <mos/mm/mmap.hpp>
#include <mos/mm/paging/paging.hpp>
#include <mos/mos_global.h>
#include <mos/tasks/process.hpp>
#include <mos/tasks/task_types.hpp>

/**
 * @brief Check if the mmap flags are valid
 *
 * @param hint_addr The hint address
 * @param mmap_flags The mmap flags
 */
static bool mmap_verify_arguments(ptr_t *hint_addr, MMapFlags mmap_flags)
{
    if ((*hint_addr % MOS_PAGE_SIZE) != 0)
    {
        pr_warn("hint address must be page-aligned");
        return false;
    }
    const bool shared = mmap_flags & MMAP_SHARED;       // when forked, shared between parent and child
    const bool map_private = mmap_flags & MMAP_PRIVATE; // when forked, make it Copy-On-Write

    if (shared == map_private)
    {
        pr_warn("mmap_file: shared and private are mutually exclusive, and one of them must be specified");
        return false;
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

ptr_t mmap_anonymous(MMContext *ctx, ptr_t hint_addr, MMapFlags flags, VMFlags VMFlags, size_t n_pages)
{
    if (!mmap_verify_arguments(&hint_addr, flags))
        return 0;

    auto vmap = cow_allocate_zeroed_pages(ctx, n_pages, hint_addr, VMFlags, flags & MMAP_EXACT);

    if (vmap.isErr())
        return vmap.getErr();

    pr_dinfo2(vmm, "allocated %zd pages at " PTR_FMT, vmap->npages, vmap->vaddr);

    const vmap_type_t type = (flags & MMAP_SHARED) ? VMAP_TYPE_SHARED : VMAP_TYPE_PRIVATE;
    vmap_finalise_init(vmap.get(), VMAP_MMAP, type);
    return vmap->vaddr;
}

ptr_t mmap_file(MMContext *ctx, ptr_t hint_addr, MMapFlags flags, VMFlags VMFlags, size_t n_pages, IO *io, off_t offset)
{
    if (!mmap_verify_arguments(&hint_addr, flags))
        return 0;

    if (offset % MOS_PAGE_SIZE != 0)
    {
        pr_warn("mmap_file: offset must be page-aligned");
        return 0;
    }

    const vmap_type_t type = (flags & MMAP_SHARED) ? VMAP_TYPE_SHARED : VMAP_TYPE_PRIVATE;

    mm_lock_context_pair(ctx);
    auto vmap = mm_get_free_vaddr_locked(ctx, n_pages, hint_addr, flags & MMAP_EXACT);
    mm_unlock_context_pair(ctx);

    if (vmap.isErr())
    {
        pr_warn("mmap_file: no free virtual address space");
        return 0;
    }

    vmap->vmflags = VMFlags;
    vmap->type = type;

    if (!io->map(vmap.get(), offset))
    {
        vmap_destroy(vmap.get());
        pr_warn("mmap_file: could not map the file: io_mmap() failed");
        return 0;
    }

    vmap_finalise_init(vmap.get(), VMAP_FILE, type);
    return vmap->vaddr;
}

bool munmap(ptr_t addr, size_t size)
{
    spinlock_acquire(&current_process->mm->mm_lock);
    vmap_t *const whole_map = vmap_obtain(current_process->mm, addr);
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

bool vm_protect(MMContext *mmctx, ptr_t addr, size_t size, VMFlags perm)
{
    MOS_ASSERT(addr % MOS_PAGE_SIZE == 0);
    size = ALIGN_UP_TO_PAGE(size);

    spinlock_acquire(&mmctx->mm_lock);
    vmap_t *const first_part = vmap_obtain(mmctx, addr);
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
        if (!to_protect->io->VerifyMMapPermissions(perm, to_protect->type == VMAP_TYPE_PRIVATE))
        {
            spinlock_release(&to_protect->lock); // permission denied
            spinlock_release(&mmctx->mm_lock);
            return false;
        }
    }

    const bool read_lost = to_protect->vmflags.test(VM_READ) && !perm.test(VM_READ);    // if we lose read permission
    const bool write_lost = to_protect->vmflags.test(VM_WRITE) && !perm.test(VM_WRITE); // if we lose write permission
    const bool exec_lost = to_protect->vmflags.test(VM_EXEC) && !perm.test(VM_EXEC);    // if we lose exec permission

    VMFlags mask = VM_NONE;
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
