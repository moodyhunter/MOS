// SPDX-License-Identifier: GPL-3.0-or-later
// Public API for the Physical Memory Manager

#include "mos/mm/physical/pmm.h"

#include "mos/mm/physical/buddy.h"
#include "mos/printk.h"

#include <stdlib.h>

static pmm_region_t reserved_regions[MOS_MAX_MEMREGIONS] = { 0 };
static size_t reserved_regions_count = 0;

phyframe_t *phyframes = (void *) MOS_PHYFRAME_ARRAY_VADDR;
size_t buddy_max_nframes = 0; // system pfn <= pfn_max

void pmm_init(size_t max_nframes)
{
    pr_info("the system has %zu frames in total", max_nframes);
    buddy_max_nframes = max_nframes;
    buddy_init(max_nframes);
}

void pmm_dump_lists(void)
{
    pr_info("Physical Memory Manager dump:");
    buddy_dump_all();
}

pfn_t pmm_allocate_frames(size_t n_frames, pmm_allocation_flags_t flags)
{
    const bool no_compound = flags & PMM_ALLOC_NO_COMPOUND;
    if (n_frames > 1 && !no_compound)
    {
        phyframe_t *frame = buddy_alloc_n_compound(n_frames);
        if (!frame)
            return MOS_INVALID_PFN;
        const pfn_t pfn = phyframe_pfn(frame);
        mos_debug(pmm, "allocated " PFN_RANGE ", %zu pages", pfn, pfn + n_frames, n_frames);
        frame->mapped_count = 0;
        return pfn;
    }

    phyframe_t *frame = buddy_alloc_n_exact(n_frames);
    if (!frame)
        return MOS_INVALID_PFN;
    const pfn_t pfn = phyframe_pfn(frame);
    mos_debug(pmm, "allocated " PFN_RANGE ", %zu pages", pfn, pfn + n_frames, n_frames);

    for (size_t i = 0; i < n_frames; i++)
    {
        phyframe_t *frame = &phyframes[pfn + i];
        frame->mapped_count = 0;
    }

    return pfn;
}

void pmm_ref_frames(pfn_t start, size_t n_pages)
{
    MOS_ASSERT_X(start + n_pages <= buddy_max_nframes, "out of bounds");
    mos_debug(pmm, "ref range: " PFN_RANGE ", %zu pages", start, start + n_pages, n_pages);

    for (size_t i = start; i < start + n_pages; i++)
    {
        phyframe_t *head = phyframe_effective_head(&phyframes[i]);
        head->mapped_count++;
    }
}

void pmm_unref_frames(pfn_t start, size_t n_pages)
{
    MOS_ASSERT_X(start + n_pages <= buddy_max_nframes, "out of bounds");
    mos_debug(pmm, "unref range: " PFN_RANGE ", %zu pages", start, start + n_pages, n_pages);

    for (size_t i = start; i < start + n_pages;)
    {
        phyframe_t *head_frame = phyframe_effective_head(&phyframes[start]);
        MOS_ASSERT(head_frame->mapped_count > 0);
        head_frame->mapped_count--;

        if (head_frame->mapped_count == 0)
        {
            linked_list_init(list_node(head_frame));
            const size_t size = pow2(head_frame->order);
            buddy_free_n(phyframe_pfn(head_frame), size / MOS_PAGE_SIZE);
            i += size;
        }
        else
        {
            i++;
        }
    }
}

void pmm_register_region(pfn_t start, size_t nframes, bool reserved)
{
    mos_debug(pmm, "registering region: " PFN_RANGE ", %zu pages", start, start + nframes - 1, nframes);

    if (!reserved)
        return;

    // we only care about reserved regions
    MOS_ASSERT_X(reserved_regions_count < MOS_MAX_MEMREGIONS, "too many memory regions");
    pmm_region_t *r = &reserved_regions[reserved_regions_count++];
    r->pfn_start = start;
    r->nframes = nframes;
    r->reserved = reserved;
    buddy_reserve_n(start, nframes);
    reserved_regions_count++;
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

    for (size_t i = 0; i < reserved_regions_count; i++)
    {
        pmm_region_t *r = &reserved_regions[i];
        if (r->reserved && r->pfn_start <= needle_pfn && needle_pfn < r->pfn_start + r->nframes)
        {
            mos_debug(pmm, "found block: " PFN_RANGE ", %zu pages", r->pfn_start, r->pfn_start + r->nframes - 1, r->nframes);
            return r;
        }
    }

    mos_debug(pmm, "no block found");
    return NULL;
}
