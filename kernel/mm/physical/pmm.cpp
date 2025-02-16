// SPDX-License-Identifier: GPL-3.0-or-later
// Public API for the Physical Memory Manager

#include "mos/mm/physical/pmm.hpp"

#include "mos/assert.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/physical/buddy.hpp"
#include "mos/platform/platform.hpp"
#include "mos/syslog/printk.hpp"

#include <mos_stdlib.hpp>
#include <mos_string.hpp>

phyframe_t *phyframes = NULL;
size_t pmm_total_frames = 0; // system pfn <= pfn_max
size_t pmm_allocated_frames = 0;
size_t pmm_reserved_frames = 0;

void pmm_init(void)
{
    pr_dinfo2(pmm, "setting up physical memory manager...");
    MOS_ASSERT_ONCE("pmm_init should only be called once");

    const size_t phyframes_npages = ALIGN_UP_TO_PAGE(platform_info->max_pfn * sizeof(phyframe_t)) / MOS_PAGE_SIZE;
    pr_dinfo2(pmm, "%zu pages required for the phyframes array with %llu pages in total", phyframes_npages, platform_info->max_pfn);

    const pmm_region_t *phyframes_region = NULL; // the region that will hold the phyframes array
    pfn_t phyframes_pfn = 0;

    // now we need to find contiguous memory for the phyframes array
    for (u32 i = 0; i < platform_info->num_pmm_regions; i++)
    {
        const pmm_region_t *const r = &platform_info->pmm_regions[i];

        if (r->reserved)
        {
            pr_dinfo2(pmm, "skipping reserved region " PFNADDR_RANGE, PFNADDR(r->pfn_start, r->pfn_start + r->nframes));
            continue;
        }

        if (r->nframes < phyframes_npages)
        {
            pr_dinfo2(pmm, "skipping region " PFNADDR_RANGE " because it's too small", PFNADDR(r->pfn_start, r->pfn_start + r->nframes));
            continue; // early out if this region is too small
        }

        phyframes_pfn = r->pfn_start;
        phyframes_region = r;

        pr_dinfo2(pmm, "using " PFNADDR_RANGE " for the phyframes array", PFNADDR(phyframes_pfn, phyframes_pfn + phyframes_npages));

        // ! initialise phyframes array
        phyframes = (phyframe_t *) pfn_va(phyframes_pfn);
        memzero(phyframes, phyframes_npages * MOS_PAGE_SIZE);
        pmm_total_frames = platform_info->max_pfn;
        buddy_init(pmm_total_frames);
        pmm_reserve_frames(phyframes_pfn, phyframes_npages);
        break;
    }

    MOS_ASSERT_X(phyframes && phyframes_region, "failed to find a region for the phyframes array");

    // add all the other regions
    for (u32 i = 0; i < platform_info->num_pmm_regions; i++)
    {
        pmm_region_t *r = &platform_info->pmm_regions[i];
        if (r == phyframes_region)
            continue;
        if (r->nframes == 0) // ???
            mos_warn_once("region " PFN_FMT " has 0 frames", r->pfn_start);
        if (r->reserved)
        {
            if (r->pfn_start >= platform_info->max_pfn)
                continue; // we ignore any reserved regions that are outside of the max_pfn
            pmm_reserve_frames(r->pfn_start, r->nframes);
        }
    }

    MOS_ASSERT_X(phyframes[0].state == phyframe::PHYFRAME_RESERVED, "phyframe 0 isn't reserved, things have gone horribly wrong");
    pr_dinfo2(pmm, "initialised");
}

void pmm_dump_lists(void)
{
    pr_info("Physical Memory Manager dump:");
    buddy_dump_all();
}

MOS_PANIC_HOOK_FEAT(pmm, pmm_dump_lists, "dump physical allocator lists");

phyframe_t *pmm_allocate_frames(size_t n_frames, pmm_allocation_flags_t flags)
{
    MOS_ASSERT(flags == PMM_ALLOC_NORMAL);
    phyframe_t *frame = buddy_alloc_n_exact(n_frames);
    if (!frame)
        return NULL;
    const pfn_t pfn = phyframe_pfn(frame);
    pr_dinfo2(pmm, "allocated " PFN_RANGE ", %zu pages", pfn, pfn + n_frames, n_frames);

    for (size_t i = 0; i < n_frames; i++)
        pfn_phyframe(pfn + i)->alloc.refcount = 0;

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
        linked_list_init(list_node(&frame->info)); // sanitize the list node
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
        pfn_phyframe(i)->alloc.refcount++;
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
        MOS_ASSERT(frame->alloc.refcount > 0);

        if (--frame->alloc.refcount == 0)
            pmm_free_frames(frame, 1);
    }
}
