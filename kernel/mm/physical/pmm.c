// SPDX-License-Identifier: GPL-3.0-or-later
// Public API for the Physical Memory Manager

#include "mos/mm/physical/pmm.h"

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/mm/mmstat.h"
#include "mos/mm/physical/buddy.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

#include <stdlib.h>

phyframe_t *phyframes = NULL;
size_t pmm_total_frames = 0; // system pfn <= pfn_max
size_t pmm_allocated_frames = 0;
size_t pmm_reserved_frames = 0;

void pmm_init(size_t max_nframes)
{
    pr_info("the system has %zu frames in total", max_nframes);
    pmm_total_frames = max_nframes;
    buddy_init(max_nframes);

#if MOS_DEBUG_FEATURE(pmm)
    panic_hook_declare(pmm_dump_lists, "Dump PMM lists");
    install_panic_hook(&pmm_dump_lists_holder);
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
    mos_debug(pmm, "allocated " PFN_RANGE ", %zu pages", pfn, pfn + n_frames, n_frames);

    for (size_t i = 0; i < n_frames; i++)
    {
        pfn_phyframe(pfn + i)->refcount = 1;
    }

    pmm_allocated_frames += n_frames;
    return frame;
}

void pmm_free_frames(phyframe_t *start_frame, size_t n_pages)
{
    const pfn_t start = phyframe_pfn(start_frame);
    mos_debug(pmm, "freeing " PFN_RANGE ", %zu pages", start, start + n_pages - 1, n_pages);
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
    mos_debug(pmm, "reserving " PFN_RANGE ", %zu pages", pfn_start, pfn_start + npages - 1, npages);
    buddy_reserve_n(pfn_start, npages);
    pmm_reserved_frames += npages;
    return pfn_start;
}

pmm_region_t *pmm_find_reserved_region(ptr_t needle)
{
    mos_debug(pmm, "looking for block containing " PTR_FMT, needle);

    const pfn_t needle_pfn = needle / MOS_PAGE_SIZE;

    for (size_t i = 0; i < platform_info->num_pmm_regions; i++)
    {
        pmm_region_t *r = &platform_info->pmm_regions[i];
        if (r->reserved && r->pfn_start <= needle_pfn && needle_pfn < r->pfn_start + r->nframes)
        {
            mos_debug(pmm, "found block: " PFN_RANGE ", %zu pages", r->pfn_start, r->pfn_start + r->nframes - 1, r->nframes);
            return r;
        }
    }

    mos_debug(pmm, "no block found");
    return NULL;
}

void _pmm_ref_phyframes(phyframe_t *frame, size_t n_pages)
{
    const pfn_t start = phyframe_pfn(frame);
    MOS_ASSERT_X(start + n_pages <= pmm_total_frames, "out of bounds");
    mos_debug(pmm, "ref range: " PFN_RANGE ", %zu pages", start, start + n_pages, n_pages);

    for (size_t i = start; i < start + n_pages; i++)
        pfn_phyframe(i)->refcount++;
}

void _pmm_unref_phyframes(phyframe_t *frame, size_t n_pages)
{
    const pfn_t start = phyframe_pfn(frame);
    MOS_ASSERT_X(start + n_pages <= pmm_total_frames, "out of bounds");
    mos_debug(pmm, "unref range: " PFN_RANGE ", %zu pages", start, start + n_pages, n_pages);

    for (pfn_t pfn = start; pfn < start + n_pages; pfn++)
    {
        phyframe_t *frame = pfn_phyframe(pfn);
        MOS_ASSERT(frame->refcount > 0);

        if (--frame->refcount == 0)
            pmm_free_frames(frame, 1);
    }
}
