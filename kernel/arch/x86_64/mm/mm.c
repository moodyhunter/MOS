// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/mm.h"

#include "mos/mm/mm.h"
#include "mos/mm/physical/pmm.h"
#include "mos/platform/platform.h"

#include <mos/syslog/printk.h>
#include <mos_stdlib.h>
#include <mos_string.h>

pfn_t phyframes_pfn = 0;
size_t phyframes_npages = 0;

void x86_initialise_phyframes_array(void)
{
    pr_dinfo2(x86_startup, "setting up physical memory manager...");
    const size_t phyframes_count = platform_info->max_pfn;

    phyframes_npages = ALIGN_UP_TO_PAGE(phyframes_count * sizeof(phyframe_t)) / MOS_PAGE_SIZE;
    pr_dinfo2(pmm, "%zu pages required for the phyframes array", phyframes_npages);

    pmm_region_t *phyframes_region = NULL; // the region that will hold the phyframes array

    // now we need to find contiguous memory for the phyframes array
    for (u32 i = 0; i < platform_info->num_pmm_regions; i++)
    {
        pmm_region_t *r = &platform_info->pmm_regions[i];

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
        phyframes = (void *) pfn_va(phyframes_pfn);

        // zero the array
        memzero(phyframes, phyframes_npages * MOS_PAGE_SIZE);
        // then we can initialize the pmm
        pmm_init(phyframes_count);
        // and finally we can reserve this region
        pmm_reserve_frames(phyframes_pfn, phyframes_npages);
        break;
    }

    MOS_ASSERT_X(phyframes_region, "failed to find a region for the phyframes array");

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

    MOS_ASSERT_X(phyframes[0].state == PHYFRAME_RESERVED, "phyframe 0 isn't reserved, things have gone horribly wrong");
}
