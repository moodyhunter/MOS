// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/lib/sync/spinlock.hpp"
#define pr_fmt(fmt) "dma: " fmt

#include "mos/mm/dma.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/paging/paging.hpp"
#include "mos/mm/physical/pmm.hpp"
#include "mos/platform/platform.hpp"
#include "mos/syslog/printk.hpp"

#include <mos_string.hpp>

static pfn_t dmabuf_do_allocate(size_t n_pages, bool do_ref)
{
    phyframe_t *frames = pmm_allocate_frames(n_pages, PMM_ALLOC_NORMAL);
    if (do_ref)
        pmm_ref(frames, n_pages);
    memset((void *) phyframe_va(frames), 0, n_pages * MOS_PAGE_SIZE);
    return phyframe_pfn(frames);
}

pfn_t dmabuf_allocate(size_t n_pages, ptr_t *vaddr)
{
    const pfn_t pfn = dmabuf_do_allocate(n_pages, true);
    auto vmap = mm_map_user_pages(current_mm, MOS_ADDR_USER_MMAP, pfn, n_pages, VM_USER_RW | VM_CACHE_DISABLED, VMAP_TYPE_SHARED, VMAP_DMA);
    if (!vmap)
    {
        pmm_unref(pfn, n_pages);
        return 0;
    }
    *vaddr = vmap->vaddr;
    pr_dinfo2(dma, "allocated %zu DMA pages at " PFN_FMT " and mapped them at " PTR_FMT, n_pages, pfn, *vaddr);
    return pfn;
}

bool dmabuf_free(ptr_t vaddr, ptr_t paddr)
{
    MOS_UNUSED(paddr);
    pr_dinfo2(dma, "freeing DMA pages at " PTR_FMT, vaddr);

    SpinLocker lock(&current_mm->mm_lock);
    vmap_t *vmap = vmap_obtain(current_mm, (ptr_t) vaddr);
    if (!vmap)
        return false;

    vmap_destroy(vmap);
    return true;
}

pfn_t dmabuf_share(void *buf, size_t size)
{
    const size_t n_pages = ALIGN_UP_TO_PAGE(size) / MOS_PAGE_SIZE;
    const pfn_t pfn = dmabuf_do_allocate(n_pages, false);
    pr_dinfo2(dma, "sharing %zu bytes at " PFN_FMT, size, pfn);

    memcpy((void *) pfn_va(pfn), buf, size);
    return pfn;
}

bool dmabuf_unshare(ptr_t phys, size_t size, void *buffer)
{
    const pfn_t pfn = phys / MOS_PAGE_SIZE;
    pr_dinfo2(dma, "unsharing %zu bytes at " PFN_FMT, size, pfn);
    memcpy(buffer, (void *) pfn_va(pfn), size);
    pmm_free_frames(pfn_phyframe(pfn), ALIGN_UP_TO_PAGE(size) / MOS_PAGE_SIZE);
    return true;
}
