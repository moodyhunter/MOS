// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/tasks/task_types.h"

#include <mos/kconfig.h>
#include <mos/lib/structures/bitmap.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/paging/pagemap.h>
#include <mos/mm/paging/paging.h>
#include <mos/mm/physical/pmm.h>
#include <mos/mos_global.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <stdlib.h>

#define PGD_FOR_VADDR(_vaddr, _um) (_vaddr >= MOS_KERNEL_START_VADDR ? &platform_info->kernel_mm : _um)

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
        platform_mm_unmap_pages(PGD_FOR_VADDR(block->vaddr, block->address_space), block->vaddr, block->npages);

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
    platform_mm_map_pages(PGD_FOR_VADDR(target_vaddr, vblock->address_space), target_vaddr, block_pfn, block->npages, block->flags);
}
// ! END: CALLBACKS

ptr_t mm_get_free_pages(mm_context_t *mmctx, size_t n_pages, ptr_t base_vaddr, valloc_flags flags)
{
    if (IS_KHEAP_VADDR(base_vaddr, n_pages))
        return kpagemap_get_free_pages(n_pages, base_vaddr, flags);

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

    mmctx = PGD_FOR_VADDR(hint_vaddr, mmctx);

    spinlock_acquire(&mmctx->mm_lock);
    const ptr_t vaddr = mm_get_free_pages(mmctx, n_pages, hint_vaddr, valloc_flags);
    if (unlikely(vaddr == 0))
    {
        mos_warn("could not find free %zd pages", n_pages);
        spinlock_release(&mmctx->mm_lock);
        return (vmblock_t){ 0 };
    }

    const vmblock_t block = { .address_space = mmctx, .vaddr = vaddr, .npages = n_pages, .flags = flags };

    const pfn_t pfn = pmm_allocate_frames(n_pages, PMM_ALLOC_NORMAL);

    if (unlikely(pfn_invalid(pfn)))
    {
        spinlock_release(&mmctx->mm_lock);
        mos_warn("could not allocate %zd physical pages", n_pages);
        return (vmblock_t){ 0 };
    }

    pmm_ref_frames(pfn, n_pages);
    mos_debug(vmm, "mapping %zd pages at " PTR_FMT " to pfn " PFN_FMT, n_pages, vaddr, pfn);
    platform_mm_map_pages(mmctx, vaddr, pfn, n_pages, flags);

    spinlock_release(&mmctx->mm_lock);

    return block;
}

vmblock_t mm_map_pages_locked(mm_context_t *mmctx, ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags)
{
    MOS_ASSERT(spinlock_is_locked(&mmctx->mm_lock));
    MOS_ASSERT(npages > 0);

    const vmblock_t block = { .address_space = mmctx, .vaddr = vaddr, .npages = npages, .flags = flags };

    mmctx = PGD_FOR_VADDR(vaddr, mmctx); // after block is initialized

    if (unlikely(mmctx->pgd == 0))
    {
        mos_warn("cannot map pages at " PTR_FMT ", pagetable is null", vaddr);
        return (vmblock_t){ 0 };
    }

    mos_debug(vmm, "mapping %zd pages at " PTR_FMT " to pfn " PFN_FMT, npages, vaddr, pfn);

    if (IS_KHEAP_VADDR(vaddr, npages))
        kpagemap_mark_used(vaddr, npages);

    pmm_ref_frames(pfn, npages);
    platform_mm_map_pages(mmctx, vaddr, pfn, npages, flags);
    return block;
}

vmblock_t mm_map_pages(mm_context_t *mmctx, ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags)
{
    spinlock_acquire(&mmctx->mm_lock);
    const vmblock_t block = mm_map_pages_locked(mmctx, vaddr, pfn, npages, flags);
    spinlock_release(&mmctx->mm_lock);
    return block;
}

vmblock_t mm_early_map_kernel_pages(ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags)
{
    MOS_ASSERT(npages > 0);
    MOS_ASSERT(vaddr >= MOS_KERNEL_START_VADDR);

    mm_context_t *mmctx = &platform_info->kernel_mm;
    const vmblock_t block = { .address_space = mmctx, .vaddr = vaddr, .npages = npages, .flags = flags };

    mos_debug(vmm, "mapping %zd pages at " PTR_FMT " to " PFN_FMT, npages, vaddr, pfn);

    spinlock_acquire(&mmctx->mm_lock);
    platform_mm_map_pages(mmctx, vaddr, pfn, npages, flags);
    spinlock_release(&mmctx->mm_lock);

    return block;
}

void mm_unmap_pages(mm_context_t *mmctx, ptr_t vaddr, size_t npages)
{
    MOS_ASSERT(npages > 0);

    mmctx = PGD_FOR_VADDR(vaddr, mmctx);
    spinlock_acquire(&mmctx->mm_lock);
    platform_mm_iterate_table(mmctx, vaddr, npages, vmm_iterate_unmap, NULL);
    spinlock_release(&mmctx->mm_lock);
}

vmblock_t mm_replace_mapping(mm_context_t *mmctx, ptr_t vaddr, pfn_t pfn, size_t npages, vm_flags flags)
{
    MOS_ASSERT(npages > 0);

    const vmblock_t block = { .address_space = mmctx, .vaddr = vaddr, .npages = npages, .flags = flags };

    mmctx = PGD_FOR_VADDR(vaddr, mmctx); // after block is initialized

    if (unlikely(mmctx->pgd == 0))
    {
        mos_warn("cannot fill pages at " PTR_FMT ", pagetable is null", vaddr);
        return (vmblock_t){ 0 };
    }

    mos_debug(vmm, "filling %zd pages at " PTR_FMT " with " PFN_FMT, npages, vaddr, pfn);

    // only unreference the physical frames
    static const vmm_iterate_unmap_flags_t umflags = {
        .do_unmap = false,
        .do_unref = true,
    };

    spinlock_acquire(&mmctx->mm_lock);
    // ref the frames first, so they don't get freed if we're [filling a page with itself]?
    // weird people do weird things
    pmm_ref_frames(pfn, npages);
    platform_mm_iterate_table(mmctx, vaddr, npages, vmm_iterate_unmap, (void *) &umflags);
    platform_mm_map_pages(mmctx, vaddr, pfn, npages, flags);
    spinlock_release(&mmctx->mm_lock);

    return block;
}

vmblock_t mm_copy_maps_locked(mm_context_t *from, ptr_t fvaddr, mm_context_t *to, ptr_t tvaddr, size_t npages, mm_copy_behavior_t behavior)
{
    MOS_ASSERT(npages > 0);
    mos_debug(vmm, "copying mapping from " PTR_FMT " to " PTR_FMT ", %zu pages", fvaddr, tvaddr, npages);

    vmblock_t result = { .address_space = to, .vaddr = tvaddr, .npages = npages };

    if (unlikely(from->pgd == 0 || to->pgd == 0))
    {
        mos_warn("cannot remap pages from " PTR_FMT " to " PTR_FMT ", pagetable is null", fvaddr, tvaddr);
        return (vmblock_t){ 0 };
    }

    platform_mm_iterate_table(from, fvaddr, npages, vmm_iterate_copymap, &result);

    return result;
}

vmblock_t mm_copy_maps(mm_context_t *from, ptr_t fvaddr, mm_context_t *to, ptr_t tvaddr, size_t npages, mm_copy_behavior_t behavior)
{

    spinlock_acquire(&from->mm_lock);
    if (to != from)
        spinlock_acquire(&to->mm_lock);
    const vmblock_t result = mm_copy_maps_locked(from, fvaddr, to, tvaddr, npages, behavior);
    if (to != from)
        spinlock_release(&to->mm_lock);
    spinlock_release(&from->mm_lock);

    return result;
}

bool mm_get_is_mapped(mm_context_t *mmctx, ptr_t vaddr)
{
    if (IS_KHEAP_VADDR(vaddr, 1))
        return kpagemap_get_mapped(vaddr);

    list_foreach(vmap_t, vmap, mmctx->mmaps)
    {
        if (vmap->blk.vaddr <= vaddr && vaddr < vmap->blk.vaddr + vmap->blk.npages * MOS_PAGE_SIZE)
            return true;
    }

    return false;
}

void mm_flag_pages(mm_context_t *mmctx, ptr_t vaddr, size_t npages, vm_flags flags)
{
    MOS_ASSERT(npages > 0);

    mmctx = PGD_FOR_VADDR(vaddr, mmctx);
    if (unlikely(mmctx->pgd == 0))
    {
        mos_warn("cannot flag pages at " PTR_FMT ", pagetable is null", vaddr);
        return;
    }

    mos_debug(vmm, "flagging %zd pages at " PTR_FMT " with flags %x", npages, vaddr, flags);

    spinlock_acquire(&mmctx->mm_lock);
    platform_mm_flag_pages(mmctx, vaddr, npages, flags);
    spinlock_release(&mmctx->mm_lock);
}

mm_context_t *mm_new_context(void)
{
    mm_context_t *mmctx = kmemcache_alloc(mm_context_cache);
    linked_list_init(&mmctx->mmaps);
    mmctx->pgd = platform_mm_create_user_pgd();
    if (unlikely(mmctx->pgd == 0))
    {
        kfree(mmctx);
        return NULL;
    }
    return mmctx;
}

void mm_destroy_user_pgd(mm_context_t *mmctx)
{
    if (unlikely(mmctx->pgd == 0))
    {
        mos_warn("cannot destroy user pgd, pagetable is null");
        return;
    }

    spinlock_acquire(&mmctx->mm_lock);
    platform_mm_destroy_user_pgd(mmctx);
}
