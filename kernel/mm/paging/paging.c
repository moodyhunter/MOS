// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.h"
#include "mos/mm/paging/pmlx/pml4.h"
#include "mos/mm/paging/table_ops.h"
#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/tasks/task_types.h"

#include <mos/kconfig.h>
#include <mos/lib/structures/bitmap.h>
#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/paging/paging.h>
#include <mos/mm/physical/pmm.h>
#include <mos/mos_global.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <stdlib.h>

static slab_t *mm_context_cache = NULL;
MOS_SLAB_AUTOINIT("mm_context", mm_context_cache, mm_context_t);

// ! BEGIN: CALLBACKS
static void vmm_iterate_copymap(const pgt_iteration_info_t *iter_info, const vmblock_t *block, pfn_t block_pfn, void *arg)
{
    vmblock_t *vblock = arg;
    if (!vblock->flags)
        vblock->flags = block->flags;
    else
        MOS_ASSERT_X(vblock->flags == block->flags, "flags mismatch");
    const ptr_t target_vaddr = vblock->vaddr + (block->vaddr - iter_info->vaddr_start); // we know that vaddr is contiguous, so their difference is the offset
    mos_debug(vmm_impl, "copymapping " PTR_FMT " -> " PFN_FMT " (npages: %zu)", target_vaddr, block_pfn, block->npages);
    pmm_ref(block_pfn, block->npages);
    // platform_mm_map_pages(PGD_FOR_VADDR(target_vaddr, vblock->mm_context), target_vaddr, block_pfn, block->npages, block->flags);
    // mm_do_map(vblock->mm_context.kernel_pml, target_vaddr, block_pfn, block->npages, block->flags);
    mos_panic("WIP");
}
// ! END: CALLBACKS

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

vmap_t *mm_alloc_pages(mm_context_t *mmctx, size_t n_pages, ptr_t hint_vaddr, valloc_flags valloc_flags, vm_flags flags)
{
    MOS_ASSERT(n_pages > 0);

    phyframe_t *frame = mm_get_free_pages(n_pages);
    if (unlikely(!frame))
    {
        spinlock_release(&mmctx->mm_lock);
        mos_warn("could not allocate %zd physical pages", n_pages);
        return NULL;
    }

    spinlock_acquire(&mmctx->mm_lock);
    vmap_t *vmap = mm_get_free_vaddr_locked(mmctx, n_pages, hint_vaddr, valloc_flags);
    if (unlikely(!vmap))
    {
        mos_warn("could not find %zd pages in the address space", n_pages);
        spinlock_release(&mmctx->mm_lock);
        mm_free_pages(frame, n_pages);
        return NULL;
    }

    const pfn_t pfn = phyframe_pfn(frame);
    mos_debug(vmm, "mapping %zd pages at " PTR_FMT " to pfn " PFN_FMT, n_pages, vmap->vaddr, pfn);

    vmap->vmflags = flags;
    mm_do_map(mmctx->pgd, vmap->vaddr, pfn, n_pages, flags);
    spinlock_release(&mmctx->mm_lock);

    // TODO: update the vmap stat
    return vmap;
}

void mm_map_pages_locked(mm_context_t *ctx, ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags)
{
    MOS_ASSERT(spinlock_is_locked(&ctx->mm_lock));
    MOS_ASSERT(npages > 0);
    mos_debug(vmm, "mapping %zd pages at " PTR_FMT " to pfn " PFN_FMT, npages, vaddr, pfn);
    mm_do_map(ctx->pgd, vaddr, pfn, npages, flags);
}

void mm_map_pages(mm_context_t *mmctx, ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags)
{
    spinlock_acquire(&mmctx->mm_lock);
    mm_map_pages_locked(mmctx, vaddr, pfn, npages, flags);
    spinlock_release(&mmctx->mm_lock);
}

vmap_t *mm_map_pages_to_user(mm_context_t *mmctx, ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags)
{
    spinlock_acquire(&mmctx->mm_lock);
    vmap_t *vmap = mm_get_free_vaddr_locked(mmctx, npages, vaddr, VALLOC_EXACT);
    if (unlikely(!vmap))
    {
        mos_warn("could not find %zd pages in the address space", npages);
        spinlock_release(&mmctx->mm_lock);
        return NULL;
    }

    mos_debug(vmm, "mapping %zd pages at " PTR_FMT " to pfn " PFN_FMT, npages, vmap->vaddr, pfn);
    vmap->vmflags = flags;
    mm_do_map(mmctx->pgd, vmap->vaddr, pfn, npages, flags);
    spinlock_release(&mmctx->mm_lock);
    return vmap;
}

void mm_unmap_pages(mm_context_t *ctx, ptr_t vaddr, size_t npages)
{
    MOS_ASSERT(npages > 0);

    spinlock_acquire(&ctx->mm_lock);
    mm_do_unmap(ctx->pgd, vaddr, npages, true);
    // TODO: remove the page table
    spinlock_release(&ctx->mm_lock);
}

void mm_replace_page_locked(mm_context_t *ctx, ptr_t vaddr, pfn_t pfn, vm_flags flags)
{
    mos_debug(vmm, "filling page at " PTR_FMT " with " PFN_FMT, vaddr, pfn);

    const pfn_t old_pfn = mm_do_get_pfn(ctx->pgd, vaddr);
    if (likely(old_pfn != 0))
        pmm_unref_one(old_pfn); // unmapped

    if (unlikely(old_pfn == pfn))
        return; // nothing to do

    pmm_ref_one(pfn);
    mm_do_map(ctx->pgd, vaddr, pfn, 1, flags);
}

vmap_t *mm_clone_vmap_locked(vmap_t *src_vmap, mm_context_t *dst_ctx, vmap_t *dst_vmap)
{
    // we allocate a new vmap if no existing one is provided
    if (!dst_vmap)
        dst_vmap = mm_get_free_vaddr_locked(dst_ctx, src_vmap->npages, src_vmap->vaddr, VALLOC_EXACT);
    else
        MOS_ASSERT(dst_vmap->npages == src_vmap->npages); // the destination vmap is not large enough

    mos_debug(vmm, "copying mapping from " PTR_FMT " to " PTR_FMT ", %zu pages", src_vmap->vaddr, dst_vmap->vaddr, src_vmap->npages);

    MOS_UNIMPLEMENTED("mm_clone_vmap_locked");
    // platform_mm_iterate_table(from, fvaddr, npages, vmm_iterate_copymap, &result);

    return dst_vmap;
}

vmap_t *mm_clone_vmap(vmap_t *src_vmap, mm_context_t *dst_ctx, vmap_t *dst_vmap)
{
    spinlock_acquire(&src_vmap->mmctx->mm_lock);
    if (src_vmap->mmctx != dst_ctx)
        spinlock_acquire(&dst_ctx->mm_lock);
    vmap_t *vmap = mm_clone_vmap_locked(src_vmap, dst_ctx, dst_vmap);
    if (src_vmap->mmctx != dst_ctx)
        spinlock_release(&dst_ctx->mm_lock);
    spinlock_release(&src_vmap->mmctx->mm_lock);
    return vmap;
}

bool mm_get_is_mapped(mm_context_t *mmctx, ptr_t vaddr)
{
    MOS_ASSERT(spinlock_is_locked(&mmctx->mm_lock));
    list_foreach(vmap_t, vmap, mmctx->mmaps)
    {
        if (vmap->vaddr <= vaddr && vaddr < vmap->vaddr + vmap->npages * MOS_PAGE_SIZE)
            return true;
    }

    return false;
}

void mm_flag_pages(mm_context_t *ctx, ptr_t vaddr, size_t npages, vm_flags flags)
{
    MOS_ASSERT(npages > 0);

    mos_debug(vmm, "flagging %zd pages at " PTR_FMT " with flags %x", npages, vaddr, flags);

    spinlock_acquire(&ctx->mm_lock);
    mm_do_flag(ctx->pgd, vaddr, npages, flags);
    spinlock_release(&ctx->mm_lock);
}

mm_context_t *mm_create_context(void)
{
    mm_context_t *mmctx = kmemcache_alloc(mm_context_cache);
    linked_list_init(&mmctx->mmaps);

    pml4_t pml4 = pml_create_table(pml4);

    // map the upper half of the address space to the kernel
    for (int i = pml4_index(MOS_KERNEL_START_VADDR); i < PML4_ENTRIES; i++)
    {
        const pml4e_t *kpml4e = &platform_info->kernel_mm->pgd.max.next.table[i];
        pml4.table[i].content = kpml4e->content;
    }

    mmctx->pgd = pgd_create(pml4);

    return mmctx;
}

void mm_destroy_context(mm_context_t *mmctx)
{
    MOS_UNUSED(mmctx);
    MOS_UNIMPLEMENTED("mm_destroy_context");
}

ptr_t mm_get_phys_addr(mm_context_t *ctx, ptr_t vaddr)
{
    pfn_t pfn = mm_do_get_pfn(ctx->pgd, vaddr);
    return pfn << PML1_SHIFT | (vaddr % MOS_PAGE_SIZE);
}
