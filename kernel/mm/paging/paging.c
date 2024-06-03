// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/io/io.h"
#include "mos/mm/mm.h"
#include "mos/mm/mmstat.h"
#include "mos/mm/paging/table_ops.h"

#include <mos/lib/structures/bitmap.h>
#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/paging/paging.h>
#include <mos/mm/physical/pmm.h>
#include <mos/mos_global.h>
#include <mos/platform/platform.h>
#include <mos/syslog/printk.h>
#include <mos_stdlib.h>

vmap_t *mm_get_free_vaddr_locked(mm_context_t *mmctx, size_t n_pages, ptr_t base_vaddr, valloc_flags flags)
{
    MOS_ASSERT_X(spinlock_is_locked(&mmctx->mm_lock), "insane mmctx->mm_lock state");
    MOS_ASSERT_X(base_vaddr < MOS_KERNEL_START_VADDR, "Use mm_get_free_pages instead");

    if (flags & VALLOC_EXACT)
    {
        const ptr_t end_vaddr = base_vaddr + n_pages * MOS_PAGE_SIZE;
        // we need to find a free area that starts at base_vaddr
        list_foreach(vmap_t, vmap, mmctx->mmaps)
        {
            const ptr_t this_vaddr = vmap->vaddr;
            const ptr_t this_end_vaddr = this_vaddr + vmap->npages * MOS_PAGE_SIZE;

            // see if this vmap overlaps with the area we want to allocate
            if (this_vaddr < end_vaddr && this_end_vaddr > base_vaddr)
            {
                // this mmap overlaps with the area we want to allocate
                // so we can't allocate here
                return NULL;
            }
        }

        // nothing seems to overlap
        return vmap_create(mmctx, base_vaddr, n_pages);
    }
    else
    {
        ptr_t retry_addr = base_vaddr;
        list_foreach(vmap_t, mmap, mmctx->mmaps)
        {
            // we've reached the end of the user address space?
            if (retry_addr + n_pages * MOS_PAGE_SIZE > MOS_KERNEL_START_VADDR)
                return NULL;

            const ptr_t this_vaddr = mmap->vaddr;
            const ptr_t this_end_vaddr = this_vaddr + mmap->npages * MOS_PAGE_SIZE;

            const ptr_t target_vaddr_end = retry_addr + n_pages * MOS_PAGE_SIZE;
            if (this_vaddr < target_vaddr_end && this_end_vaddr > retry_addr)
            {
                // this mmap overlaps with the area we want to allocate
                // so we can't allocate here
                retry_addr = this_end_vaddr; // try the next area
            }

            if (retry_addr + n_pages * MOS_PAGE_SIZE <= this_vaddr)
            {
                // we've found a free area that is large enough
                return vmap_create(mmctx, retry_addr, n_pages);
            }
        }

        // we've reached the end of the list, no matter it's empty or not
        if (retry_addr + n_pages * MOS_PAGE_SIZE <= MOS_KERNEL_START_VADDR)
            return vmap_create(mmctx, retry_addr, n_pages);
        else
            return NULL;
    }
}

void mm_map_kernel_pages(mm_context_t *mmctx, ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags)
{
    MOS_ASSERT(vaddr >= MOS_KERNEL_START_VADDR);
    MOS_ASSERT(npages > 0);
    spinlock_acquire(&mmctx->mm_lock);
    pr_dinfo2(vmm, "mapping %zd pages at " PTR_FMT " to pfn " PFN_FMT, npages, vaddr, pfn);
    mm_do_map(mmctx->pgd, vaddr, pfn, npages, flags, false);
    spinlock_release(&mmctx->mm_lock);
}

vmap_t *mm_map_user_pages(mm_context_t *mmctx, ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags, valloc_flags vaflags, vmap_type_t type, vmap_content_t content)
{
    spinlock_acquire(&mmctx->mm_lock);
    vmap_t *vmap = mm_get_free_vaddr_locked(mmctx, npages, vaddr, vaflags);
    if (unlikely(!vmap))
    {
        mos_warn("could not find %zd pages in the address space", npages);
        spinlock_release(&mmctx->mm_lock);
        return NULL;
    }

    pr_dinfo2(vmm, "mapping %zd pages at " PTR_FMT " to pfn " PFN_FMT, npages, vmap->vaddr, pfn);
    vmap->vmflags = flags;
    vmap->stat.regular = npages;
    mm_do_map(mmctx->pgd, vmap->vaddr, pfn, npages, flags, false);
    spinlock_release(&mmctx->mm_lock);
    vmap_finalise_init(vmap, content, type);
    return vmap;
}

void mm_replace_page_locked(mm_context_t *ctx, ptr_t vaddr, pfn_t pfn, vm_flags flags)
{
    vaddr = ALIGN_DOWN_TO_PAGE(vaddr);
    pr_dinfo2(vmm, "filling page at " PTR_FMT " with " PFN_FMT, vaddr, pfn);

    const pfn_t old_pfn = mm_do_get_pfn(ctx->pgd, vaddr);

    if (unlikely(old_pfn == pfn))
    {
        mos_warn("trying to replace page at " PTR_FMT " with the same page " PFN_FMT, vaddr, pfn);
        return;
    }

    if (likely(old_pfn))
        pmm_unref_one(old_pfn); // unmapped

    pmm_ref_one(pfn);
    mm_do_map(ctx->pgd, vaddr, pfn, 1, flags, false);
}

vmap_t *mm_clone_vmap_locked(vmap_t *src_vmap, mm_context_t *dst_ctx)
{
    vmap_t *dst_vmap = mm_get_free_vaddr_locked(dst_ctx, src_vmap->npages, src_vmap->vaddr, VALLOC_EXACT);

    if (unlikely(!dst_vmap))
    {
        mos_warn("could not find %zd pages in the address space", src_vmap->npages);
        return NULL;
    }

    pr_dinfo2(vmm, "copying mapping from " PTR_FMT ", %zu pages", src_vmap->vaddr, src_vmap->npages);
    mm_do_copy(src_vmap->mmctx->pgd, dst_vmap->mmctx->pgd, src_vmap->vaddr, src_vmap->npages);

    dst_vmap->vmflags = src_vmap->vmflags;
    dst_vmap->io = src_vmap->io;
    dst_vmap->io_offset = src_vmap->io_offset;
    dst_vmap->content = src_vmap->content;
    dst_vmap->type = src_vmap->type;
    dst_vmap->stat = src_vmap->stat;
    dst_vmap->on_fault = src_vmap->on_fault;

    if (src_vmap->io)
        io_ref(src_vmap->io);

    return dst_vmap;
}

bool mm_get_is_mapped_locked(mm_context_t *mmctx, ptr_t vaddr)
{
    MOS_ASSERT(spinlock_is_locked(&mmctx->mm_lock));
    list_foreach(vmap_t, vmap, mmctx->mmaps)
    {
        if (vmap->vaddr <= vaddr && vaddr < vmap->vaddr + vmap->npages * MOS_PAGE_SIZE)
            return true;
    }

    return false;
}

void mm_flag_pages_locked(mm_context_t *ctx, ptr_t vaddr, size_t npages, vm_flags flags)
{
    MOS_ASSERT(npages > 0);
    MOS_ASSERT(spinlock_is_locked(&ctx->mm_lock));
    pr_dinfo2(vmm, "flagging %zd pages at " PTR_FMT " with flags %x", npages, vaddr, flags);
    mm_do_flag(ctx->pgd, vaddr, npages, flags);
}

ptr_t mm_get_phys_addr(mm_context_t *ctx, ptr_t vaddr)
{
    pfn_t pfn = mm_do_get_pfn(ctx->pgd, vaddr);
    return pfn << PML1_SHIFT | (vaddr % MOS_PAGE_SIZE);
}
