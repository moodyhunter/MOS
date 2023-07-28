// SPDX-License-Identifier: GPL-3.0-or-later
// Public API for the Physical Memory Manager

#include "mos/mm/physical/pmm.h"

#include "mos/mm/physical/buddy.h"
#include "mos/panic.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

#include <stdlib.h>

phyframe_t *phyframes = NULL;
size_t buddy_max_nframes = 0; // system pfn <= pfn_max

void pmm_init(size_t max_nframes)
{
    pr_info("the system has %zu frames in total", max_nframes);
    buddy_max_nframes = max_nframes;
    buddy_init(max_nframes);

#if MOS_DEBUG_FEATURE(pmm)
    declare_panic_hook(pmm_dump_lists, "Dump PMM lists");
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
    const bool compound = flags & PMM_ALLOC_COMPOUND;
    if (n_frames > 1 && compound)
    {
        phyframe_t *frame = buddy_alloc_n_compound(n_frames);
        if (!frame)
            return NULL;
        const pfn_t pfn = phyframe_pfn(frame);
        mos_debug(pmm, "allocated " PFN_RANGE ", %zu pages", pfn, pfn + n_frames, n_frames);
        frame->refcount = 0;
        return frame;
    }

    phyframe_t *frame = buddy_alloc_n_exact(n_frames);
    if (!frame)
        return NULL;
    const pfn_t pfn = phyframe_pfn(frame);
    mos_debug(pmm, "allocated " PFN_RANGE ", %zu pages", pfn, pfn + n_frames, n_frames);

    for (size_t i = 0; i < n_frames; i++)
    {
        phyframe_t *frame = &phyframes[pfn + i];
        frame->refcount = 1;
    }

    return frame;
}

void pmm_ref_frames(pfn_t start, size_t n_pages)
{
    MOS_ASSERT_X(start + n_pages <= buddy_max_nframes, "out of bounds");
    mos_debug(pmm, "ref range: " PFN_RANGE ", %zu pages", start, start + n_pages, n_pages);

    for (size_t i = start; i < start + n_pages; i++)
    {
        phyframe_t *head = phyframe_effective_head(&phyframes[i]);
        head->refcount++;
    }
}

void pmm_unref_frames(pfn_t start, size_t n_pages)
{
    MOS_ASSERT_X(start + n_pages <= buddy_max_nframes, "out of bounds");
    mos_debug(pmm, "unref range: " PFN_RANGE ", %zu pages", start, start + n_pages, n_pages);

    for (size_t ith = start; ith < start + n_pages;)
    {
        phyframe_t *head_frame = phyframe_effective_head(&phyframes[ith]);
        MOS_ASSERT(head_frame->refcount > 0);

        const size_t compound_size = pow2((size_t) head_frame->order); // compound_size, or 1 for non-compound frames
        const size_t npages_left = n_pages - (ith - start);

        const size_t npages_to_unref = MIN(compound_size, npages_left);

        ith += npages_to_unref;
        head_frame->refcount -= npages_to_unref;

        if (head_frame->refcount == 0)
        {
            const pfn_t start_pfn = phyframe_pfn(head_frame);
            mos_debug(pmm, "freeing " PFN_RANGE, start_pfn, start_pfn + compound_size - 1);
            linked_list_init(list_node(head_frame)); // sanitize the list node
            buddy_free_n(phyframe_pfn(head_frame), compound_size);
        }
    }
}

phyframe_t *pmm_ref_frame(phyframe_t *frame)
{
    MOS_ASSERT(!frame->compound_tail && frame->compound_head == NULL);
    frame->refcount++;
    return frame; // for convenience
}

void pmm_unref_frame(phyframe_t *frame)
{
    MOS_ASSERT(!frame->compound_tail && frame->compound_head == NULL);
    MOS_ASSERT(frame->refcount > 0);
    frame->refcount--;

    if (frame->refcount == 0)
    {
        linked_list_init(list_node(frame)); // sanitize the list node
        pfn_t pfn = phyframe_pfn(frame);
        mos_debug(pmm, "freeing " PFN_FMT, pfn);
        buddy_free_n(pfn, 1);
    }
}

pfn_t pmm_reserve_frames(pfn_t pfn_start, size_t npages)
{
    MOS_ASSERT_X(pfn_start + npages <= buddy_max_nframes, "out of bounds: " PFN_RANGE ", %zu pages", pfn_start, pfn_start + npages - 1, npages);
    mos_debug(pmm, "reserving " PFN_RANGE ", %zu pages", pfn_start, pfn_start + npages - 1, npages);
    buddy_reserve_n(pfn_start, npages);
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
