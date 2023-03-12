// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/memops.h"

#include "lib/liballoc.h"
#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging/paging.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

static vmblock_t zero_block;

void mos_kernel_mm_init(void)
{
    // zero fill on demand (read-only)
    zero_block = mm_alloc_pages(current_cpu->pagetable, 1, PGALLOC_HINT_KHEAP, VM_RW);
    memzero((void *) zero_block.vaddr, MOS_PAGE_SIZE);

    liballoc_init();
#if MOS_DEBUG_FEATURE(liballoc)
    declare_panic_hook(liballoc_dump);
    install_panic_hook(&liballoc_dump_holder);
#endif
#if MOS_DEBUG_FEATURE(pmm)
    declare_panic_hook(pmm_dump);
    install_panic_hook(&pmm_dump_holder);
#endif

    pmm_dump();
    pmm_switch_to_kheap();
}

// !! This function is called by liballoc, not intended to be called by anyone else !!
void *liballoc_alloc_page(size_t npages)
{
    if (unlikely(npages <= 0))
    {
        mos_warn("allocating negative or zero pages");
        return NULL;
    }

    vmblock_t block = mm_alloc_pages(current_cpu->pagetable, npages, PGALLOC_HINT_KHEAP, VM_RW);
    if (block.vaddr == 0)
        return NULL;

    MOS_ASSERT_X(block.vaddr >= MOS_ADDR_KERNEL_HEAP, "only use this function to free kernel heap pages");

    if (unlikely(block.npages < npages))
        mos_warn("failed to allocate %zu pages", npages);

    return (void *) block.vaddr;
}

// !! This function is called by liballoc, not intended to be called by anyone else !!
bool liballoc_free_page(void *vptr, size_t npages)
{
    MOS_ASSERT_X(vptr >= (void *) MOS_ADDR_KERNEL_HEAP, "only use this function to free kernel heap pages");

    if (unlikely(vptr == NULL))
    {
        mos_warn("freeing NULL pointer");
        return false;
    }
    if (unlikely(npages <= 0))
    {
        mos_warn("freeing negative or zero pages");
        return false;
    }

    // TODO: this is a hack
    vmblock_t block = mm_get_block_info(current_cpu->pagetable, (uintptr_t) vptr, npages);
    mm_free_pages(current_cpu->pagetable, block);
    return true;
}

vmblock_t mm_alloc_zeroed_pages(paging_handle_t handle, size_t npages, pgalloc_hints hints, vm_flags flags)
{
    vmblock_t free_pages = mm_get_free_pages(handle, npages, hints);
    if (free_pages.npages < npages)
    {
        mos_warn("failed to allocate %zu pages", npages);
        return (vmblock_t){ 0 };
    }

    // zero fill the pages
    for (size_t i = 0; i < npages; i++)
    {
        // actually, zero_block is always accessible, using [handle] == using [current_cpu->pagetable] as source
        vmblock_t this_block = {
            .vaddr = free_pages.vaddr + i * MOS_PAGE_SIZE,
            .npages = 1,
            .flags = VM_READ | VM_USER,
            .pblocks = zero_block.pblocks,
        };
        mm_map_allocated_pages(handle, this_block);
    }

    // make the pages read-only (because for now, they are mapped to zero_block)
    platform_mm_flag_pages(handle, free_pages.vaddr, npages, VM_READ | ((flags & VM_USER) ? VM_USER : 0));
    free_pages.flags = flags; // but set the desired flags correctly
    return free_pages;
}

vmblock_t mm_alloc_zeroed_pages_at(paging_handle_t handle, uintptr_t vaddr, size_t npages, vm_flags flags)
{
    if (mm_get_is_mapped(handle, vaddr))
    {
        mos_warn("failed to allocate %zu pages at %p: already mapped", npages, (void *) vaddr);
        return (vmblock_t){ 0 };
    }

    // zero fill the pages
    for (size_t i = 0; i < npages; i++)
    {
        // actually, zero_block is always accessible, using [handle] == using [current_cpu->pagetable] as source
        mm_copy_maps(current_cpu->pagetable, zero_block.vaddr, handle, vaddr + i * MOS_PAGE_SIZE, 1);
    }

    // make the pages read-only (because for now, they are mapped to zero_block)
    platform_mm_flag_pages(handle, vaddr, npages, VM_READ | ((flags & VM_USER) ? VM_USER : 0));
    return (vmblock_t){ .vaddr = vaddr, .npages = npages, .flags = flags };
}

vmblock_t mm_map_proxy_space(paging_handle_t src, uintptr_t srcvaddr, size_t npages)
{
    const vmblock_t proxy_blk = mm_get_free_pages(current_cpu->pagetable, npages, PGALLOC_HINT_KHEAP);
    mm_copy_maps(src, srcvaddr, current_cpu->pagetable, proxy_blk.vaddr, npages);
    return proxy_blk;
}

void mm_unmap_proxy_space(vmblock_t proxy_blk)
{
    mm_unmap_pages(current_cpu->pagetable, proxy_blk.vaddr, proxy_blk.npages);
}

void mm_copy_pages(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t n_pages)
{
    paging_handle_t current_handle = current_cpu->pagetable;
    if (fvaddr >= MOS_KERNEL_START_VADDR && tvaddr >= MOS_KERNEL_START_VADDR)
    {
        // both addresses are in kernel space
        memcpy((void *) tvaddr, (void *) fvaddr, n_pages * MOS_PAGE_SIZE);
    }
    else if (fvaddr >= MOS_KERNEL_START_VADDR)
    {
        // from kernel space to user space
        if (to.pgd == current_handle.pgd)
        {
            // if the target is the current pagetable
            memcpy((void *) tvaddr, (void *) fvaddr, n_pages * MOS_PAGE_SIZE);
        }
        else
        {
            // otherwise, make a proxy block
            const vmblock_t proxy_blk = mm_map_proxy_space(to, tvaddr, n_pages);
            memcpy((void *) proxy_blk.vaddr, (void *) fvaddr, n_pages * MOS_PAGE_SIZE);
            mm_unmap_proxy_space(proxy_blk);
        }
    }
    else if (tvaddr >= MOS_KERNEL_START_VADDR)
    {
        // from user space to kernel space
        if (from.pgd == current_handle.pgd)
        {
            // if the source is the current pagetable
            memcpy((void *) tvaddr, (void *) fvaddr, n_pages * MOS_PAGE_SIZE);
        }
        else
        {
            // otherwise, make a proxy block
            const vmblock_t proxy_blk = mm_map_proxy_space(from, fvaddr, n_pages);
            memcpy((void *) tvaddr, (void *) proxy_blk.vaddr, n_pages * MOS_PAGE_SIZE);
            mm_unmap_proxy_space(proxy_blk);
        }
    }
    else
    {
        // both addresses are in user space
        if (from.pgd == current_handle.pgd)
        {
            if (to.pgd == current_handle.pgd)
            {
                // although both addresses are in user space, they are both the current pagetable
                memcpy((void *) tvaddr, (void *) fvaddr, n_pages * MOS_PAGE_SIZE);
            }
            else
            {
                // only the source is the current pagetable
                const vmblock_t proxy_blk = mm_map_proxy_space(to, tvaddr, n_pages);
                memcpy((void *) proxy_blk.vaddr, (void *) fvaddr, n_pages * MOS_PAGE_SIZE);
                mm_unmap_proxy_space(proxy_blk);
            }
        }
        else if (to.pgd == current_handle.pgd)
        {
            // only the target is the current pagetable
            const vmblock_t proxy_blk = mm_map_proxy_space(from, fvaddr, n_pages);
            memcpy((void *) tvaddr, (void *) proxy_blk.vaddr, n_pages * MOS_PAGE_SIZE);
            mm_unmap_proxy_space(proxy_blk);
        }
        else
        {
            // neither is the current pagetable
            const vmblock_t proxy_blk_from = mm_map_proxy_space(from, fvaddr, n_pages);
            const vmblock_t proxy_blk_to = mm_map_proxy_space(to, tvaddr, n_pages);
            memcpy((void *) proxy_blk_to.vaddr, (void *) proxy_blk_from.vaddr, n_pages * MOS_PAGE_SIZE);
            mm_unmap_proxy_space(proxy_blk_from);
            mm_unmap_proxy_space(proxy_blk_to);
        }
    }
}
