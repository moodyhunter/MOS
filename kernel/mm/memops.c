// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/memops.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/types.h"

vmblock_t mm_map_proxy_space(paging_handle_t src, uintptr_t srcvaddr, size_t npages)
{
    const vmblock_t proxy_blk = platform_mm_get_free_pages(current_cpu->pagetable, npages, PGALLOC_HINT_USERSPACE);
    platform_mm_copy_maps(src, srcvaddr, current_cpu->pagetable, proxy_blk.vaddr, npages);
    return proxy_blk;
}

void mm_unmap_proxy_space(vmblock_t proxy_blk)
{
    platform_mm_unmap_pages(current_cpu->pagetable, proxy_blk.vaddr, proxy_blk.npages);
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
        if (to.ptr == current_handle.ptr)
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
        if (from.ptr == current_handle.ptr)
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
        if (from.ptr == current_handle.ptr)
        {
            if (to.ptr == current_handle.ptr)
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
        else if (to.ptr == current_handle.ptr)
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
