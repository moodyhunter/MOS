// SPDX-License-Identifier: GPL-3.0-or-later
// Public API for the Physical Memory Manager

#include "mos/mm/physical/pmm.h"

#include "mos/mm/physical/buddy.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

#include <mos_stdlib.h>

#if MOS_DEBUG_FEATURE(pmm)
#include "mos/panic.h" // for panic_hook_declare, panic_hook_install
#endif

phyframe_t *phyframes = NULL;
size_t pmm_total_frames = 0; // system pfn <= pfn_max
size_t pmm_allocated_frames = 0;
size_t pmm_reserved_frames = 0;

void pmm_init(size_t max_nframes)
{
    pr_dinfo(pmm, "the system has %zu frames in total", max_nframes);
    pmm_total_frames = max_nframes;
    buddy_init(max_nframes);

#if MOS_DEBUG_FEATURE(pmm)
    panic_hook_declare(pmm_dump_lists, "Dump PMM lists");
    panic_hook_install(&pmm_dump_lists_holder);
    pmm_dump_lists();
#endif
}

void pmm_dump_lists(void)
{
    pr_info("Physical Memory Manager dump:");
    buddy_dump_all();
}

phyframe_t *pmm_allocate_frames(size_t n_frames, pmm_allocation_flags_t flags)
{
    MOS_ASSERT(flags == PMM_ALLOC_NORMAL);
    phyframe_t *frame = buddy_alloc_n_exact(n_frames);
    if (!frame)
        return NULL;
    const pfn_t pfn = phyframe_pfn(frame);
    pr_dinfo2(pmm, "allocated " PFN_RANGE ", %zu pages", pfn, pfn + n_frames, n_frames);

    for (size_t i = 0; i < n_frames; i++)
        pfn_phyframe(pfn + i)->allocated_refcount = 0;

    pmm_allocated_frames += n_frames;
    return frame;
}

void pmm_free_frames(phyframe_t *start_frame, size_t n_pages)
{
    const pfn_t start = phyframe_pfn(start_frame);
    pr_dinfo2(pmm, "freeing " PFN_RANGE ", %zu pages", start, start + n_pages - 1, n_pages);
    for (pfn_t pfn = start; pfn < start + n_pages; pfn++)
    {
        phyframe_t *frame = pfn_phyframe(pfn);
        linked_list_init(list_node(frame)); // sanitize the list node
        buddy_free_n(phyframe_pfn(frame), 1);
    }

    pmm_allocated_frames -= n_pages;
}

pfn_t pmm_reserve_frames(pfn_t pfn_start, size_t npages)
{
    MOS_ASSERT_X(pfn_start + npages <= pmm_total_frames, "out of bounds: " PFN_RANGE ", %zu pages", pfn_start, pfn_start + npages - 1, npages);
    pr_dinfo2(pmm, "reserving " PFN_RANGE ", %zu pages", pfn_start, pfn_start + npages - 1, npages);
    buddy_reserve_n(pfn_start, npages);
    pmm_reserved_frames += npages;
    return pfn_start;
}

pmm_region_t *pmm_find_reserved_region(ptr_t needle)
{
    pr_dinfo2(pmm, "looking for block containing " PTR_FMT, needle);

    const pfn_t needle_pfn = needle / MOS_PAGE_SIZE;

    for (size_t i = 0; i < platform_info->num_pmm_regions; i++)
    {
        pmm_region_t *r = &platform_info->pmm_regions[i];
        if (r->reserved && r->pfn_start <= needle_pfn && needle_pfn < r->pfn_start + r->nframes)
        {
            pr_dinfo2(pmm, "found block: " PFN_RANGE ", %zu pages", r->pfn_start, r->pfn_start + r->nframes - 1, r->nframes);
            return r;
        }
    }

    pr_dinfo2(pmm, "no block found");
    return NULL;
}

phyframe_t *_pmm_ref_phyframes(phyframe_t *frame, size_t n_pages)
{
    const pfn_t start = phyframe_pfn(frame);
    MOS_ASSERT_X(start + n_pages <= pmm_total_frames, "out of bounds");
    pr_dinfo2(pmm, "ref range: " PFN_RANGE ", %zu pages", start, start + n_pages, n_pages);

    for (size_t i = start; i < start + n_pages; i++)
        pfn_phyframe(i)->allocated_refcount++;
    return frame;
}

void _pmm_unref_phyframes(phyframe_t *frame, size_t n_pages)
{
    const pfn_t start = phyframe_pfn(frame);
    MOS_ASSERT_X(start + n_pages <= pmm_total_frames, "out of bounds");
    pr_dinfo2(pmm, "unref range: " PFN_RANGE ", %zu pages", start, start + n_pages, n_pages);

    for (pfn_t pfn = start; pfn < start + n_pages; pfn++)
    {
        phyframe_t *frame = pfn_phyframe(pfn);
        MOS_ASSERT(frame->allocated_refcount > 0);

        if (--frame->allocated_refcount == 0)
            pmm_free_frames(frame, 1);
    }
}
