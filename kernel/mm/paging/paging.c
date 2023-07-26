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

typedef struct
{
    bool do_unmap;
    bool do_unref;
} vmm_iterate_unmap_flags_t;

static slab_t *mm_context_cache = NULL;
MOS_SLAB_AUTOINIT("mm_context", mm_context_cache, mm_context_t);

// ! BEGIN: CALLBACKS
static void vmm_iterate_unmap(const pgt_iteration_info_t *iter_info, const vmblock_t *block, pfn_t block_pfn, void *arg)
{
    MOS_UNUSED(iter_info);

    const bool do_unmap = arg ? ((vmm_iterate_unmap_flags_t *) arg)->do_unmap : true;
    const bool do_unref = arg ? ((vmm_iterate_unmap_flags_t *) arg)->do_unref : true;

    mos_debug(vmm_impl, "unmapping " PTR_FMT " -> " PFN_FMT " (npages: %zu)", block->vaddr, block_pfn, block->npages);

    if (do_unmap)
        // platform_mm_unmap_pages(block->mm_context, block->vaddr, block->npages);
        MOS_UNREACHABLE();

    if (do_unref)
        pmm_unref_frames(block_pfn, block->npages);
}

static void vmm_iterate_copymap(const pgt_iteration_info_t *iter_info, const vmblock_t *block, pfn_t block_pfn, void *arg)
{
    vmblock_t *vblock = arg;
    if (!vblock->flags)
        vblock->flags = block->flags;
    else
        MOS_ASSERT_X(vblock->flags == block->flags, "flags mismatch");
    const ptr_t target_vaddr = vblock->vaddr + (block->vaddr - iter_info->vaddr_start); // we know that vaddr is contiguous, so their difference is the offset
    mos_debug(vmm_impl, "copymapping " PTR_FMT " -> " PFN_FMT " (npages: %zu)", target_vaddr, block_pfn, block->npages);
    pmm_ref_frames(block_pfn, block->npages);
    // platform_mm_map_pages(PGD_FOR_VADDR(target_vaddr, vblock->mm_context), target_vaddr, block_pfn, block->npages, block->flags);
    // mm_do_map(vblock->mm_context.kernel_pml, target_vaddr, block_pfn, block->npages, block->flags);
    mos_panic("WIP");
}
// ! END: CALLBACKS

ptr_t mm_get_free_vaddr(mm_context_t *mmctx, size_t n_pages, ptr_t base_vaddr, valloc_flags flags)
{
    MOS_ASSERT_X(base_vaddr < MOS_KERNEL_START_VADDR, "Use mm_get_free_pages instead");

    // userspace areas, we traverse the mmctx->mmaps
    MOS_ASSERT_X(spinlock_is_locked(&mmctx->mm_lock), "insane mmctx->mm_lock state");

    if (flags & VALLOC_EXACT)
    {
        const ptr_t end_vaddr = base_vaddr + n_pages * MOS_PAGE_SIZE;
        // we need to find a free area that starts at base_vaddr
        list_foreach(vmap_t, vmap, mmctx->mmaps)
        {
            const ptr_t this_vaddr = vmap->blk.vaddr;
            const ptr_t this_end_vaddr = this_vaddr + vmap->blk.npages * MOS_PAGE_SIZE;

            // see if this mmap overlaps with the area we want to allocate
            if (this_vaddr < end_vaddr && this_end_vaddr > base_vaddr)
            {
                // this mmap overlaps with the area we want to allocate
                // so we can't allocate here
                return 0;
            }
        }

        // nothing seems to overlap
        return base_vaddr;
    }
    else
    {
        ptr_t retry_addr = base_vaddr;
        list_foreach(vmap_t, mmap, mmctx->mmaps)
        {
            if (retry_addr + n_pages * MOS_PAGE_SIZE > MOS_KERNEL_START_VADDR)
            {
                // we've reached the end of the user address space
                return 0;
            }

            const ptr_t this_vaddr = mmap->blk.vaddr;
            const ptr_t this_end_vaddr = this_vaddr + mmap->blk.npages * MOS_PAGE_SIZE;

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
                return retry_addr;
            }
        }

        // empty list, or we've reached the end of the list
        if (retry_addr + n_pages * MOS_PAGE_SIZE <= MOS_KERNEL_START_VADDR)
            return retry_addr;
        else
            return 0;
    }
}

vmblock_t mm_alloc_pages(mm_context_t *mmctx, size_t n_pages, ptr_t hint_vaddr, valloc_flags valloc_flags, vm_flags flags)
{
    MOS_ASSERT(n_pages > 0);

    spinlock_acquire(&mmctx->mm_lock);
    const ptr_t vaddr = mm_get_free_vaddr(mmctx, n_pages, hint_vaddr, valloc_flags);
    if (unlikely(vaddr == 0))
    {
        mos_warn("could not find free %zd pages", n_pages);
        spinlock_release(&mmctx->mm_lock);
        return (vmblock_t){ 0 };
    }

    const vmblock_t block = { .address_space = mmctx, .vaddr = vaddr, .npages = n_pages, .flags = flags };

    const phyframe_t *frame = pmm_allocate_frames(n_pages, PMM_ALLOC_NORMAL);

    if (unlikely(!frame))
    {
        spinlock_release(&mmctx->mm_lock);
        mos_warn("could not allocate %zd physical pages", n_pages);
        return (vmblock_t){ 0 };
    }

    const pfn_t pfn = phyframe_pfn(frame);
    mos_debug(vmm, "mapping %zd pages at " PTR_FMT " to pfn " PFN_FMT, n_pages, vaddr, pfn);
    mm_do_map(mmctx->pgd, vaddr, pfn, n_pages, flags);

    spinlock_release(&mmctx->mm_lock);

    return block;
}

vmblock_t mm_map_pages_locked(mm_context_t *ctx, ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags)
{
    MOS_ASSERT(spinlock_is_locked(&ctx->mm_lock));
    MOS_ASSERT(npages > 0);

    const vmblock_t block = { .address_space = ctx, .vaddr = vaddr, .npages = npages, .flags = flags };

    mos_debug(vmm, "mapping %zd pages at " PTR_FMT " to pfn " PFN_FMT, npages, vaddr, pfn);

    pmm_ref_frames(pfn, npages);
    mm_do_map(ctx->pgd, vaddr, pfn, npages, flags);
    return block;
}

vmblock_t mm_map_pages(mm_context_t *mmctx, ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags)
{
    spinlock_acquire(&mmctx->mm_lock);
    const vmblock_t block = mm_map_pages_locked(mmctx, vaddr, pfn, npages, flags);
    spinlock_release(&mmctx->mm_lock);
    return block;
}

void mm_unmap_pages(mm_context_t *ctx, ptr_t vaddr, size_t npages)
{
    MOS_ASSERT(npages > 0);

    spinlock_acquire(&ctx->mm_lock);
    mm_do_unmap(ctx->pgd, vaddr, npages);
    // TODO: remove the page table
    spinlock_release(&ctx->mm_lock);
}

vmblock_t mm_replace_page(mm_context_t *ctx, ptr_t vaddr, pfn_t pfn, vm_flags flags)
{
    const vmblock_t block = { .address_space = ctx, .vaddr = vaddr, .flags = flags };

    mos_debug(vmm, "filling page at " PTR_FMT " with " PFN_FMT, vaddr, pfn);

    spinlock_acquire(&ctx->mm_lock);
    // ref the frames first, so they don't get freed if we're [filling a page with itself]?
    // weird people do weird things
    pmm_ref_frames(pfn, 1);
    const pfn_t old_pfn = mm_do_get_pfn(ctx->pgd, vaddr);
    mm_do_map(ctx->pgd, vaddr, pfn, 1, flags);
    pmm_unref_frames(old_pfn, 1);
    spinlock_release(&ctx->mm_lock);

    return block;
}

vmblock_t mm_copy_maps_locked(mm_context_t *from, ptr_t fvaddr, mm_context_t *to, ptr_t tvaddr, size_t npages)
{
    MOS_UNIMPLEMENTED("mm_copy_maps_locked is a stub");
    MOS_ASSERT(npages > 0);
    mos_debug(vmm, "copying mapping from " PTR_FMT " to " PTR_FMT ", %zu pages", fvaddr, tvaddr, npages);

    vmblock_t result = { .address_space = to, .vaddr = tvaddr, .npages = npages };

    if (unlikely(!from || !to))
    {
        mos_warn("cannot remap pages from " PTR_FMT " to " PTR_FMT ", pagetable is null", fvaddr, tvaddr);
        return (vmblock_t){ 0 };
    }

    platform_mm_iterate_table(from, fvaddr, npages, vmm_iterate_copymap, &result);

    return result;
}

vmblock_t mm_copy_maps(mm_context_t *from, ptr_t fvaddr, mm_context_t *to, ptr_t tvaddr, size_t npages)
{
    spinlock_acquire(&from->mm_lock);
    if (to != from)
        spinlock_acquire(&to->mm_lock);
    const vmblock_t result = mm_copy_maps_locked(from, fvaddr, to, tvaddr, npages);
    if (to != from)
        spinlock_release(&to->mm_lock);
    spinlock_release(&from->mm_lock);

    return result;
}

bool mm_get_is_mapped(mm_context_t *mmctx, ptr_t vaddr)
{
    list_foreach(vmap_t, vmap, mmctx->mmaps)
    {
        if (vmap->blk.vaddr <= vaddr && vaddr < vmap->blk.vaddr + vmap->blk.npages * MOS_PAGE_SIZE)
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
    for (int i = pml4_index(MOS_KERNEL_START_VADDR); i < MOS_PLATFORM_PML4_NPML3; i++)
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
    return pfn << MOS_PLATFORM_PML1_SHIFT | (vaddr % MOS_PAGE_SIZE);
}
