// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/mm.h"

#include "mos/boot/startup.h"
#include "mos/mm/paging/paging.h"
#include "mos/mm/physical/pmm.h"
#include "mos/platform/platform.h"
#include "mos/x86/x86_platform.h"

#include <mos/kconfig.h>
#include <mos/printk.h>
#include <stdlib.h>
#include <string.h>

pfn_t phyframes_pfn = 0;
size_t phyframes_npages = 0;

void x86_find_and_initialise_phyframes(void)
{
    const size_t phyframes_count = platform_info->max_pfn;

    phyframes_npages = ALIGN_UP_TO_PAGE(phyframes_count * sizeof(phyframe_t)) / MOS_PAGE_SIZE;
    mos_debug(pmm, "%zu pages required for the phyframes array", phyframes_npages);

    pmm_region_t *phyframes_region = NULL; // the region that will hold the phyframes array

    // now we need to find contiguous memory for the phyframes array
    for (u32 i = 0; i < platform_info->num_pmm_regions; i++)
    {
        pmm_region_t *r = &platform_info->pmm_regions[i];

        if (r->reserved)
            continue;

        if (r->nframes < phyframes_npages)
            continue;

        phyframes_region = r;

        phyframes_pfn = r->pfn_start;
        phyframes_pfn = MAX(MOS_KERNEL_PFN(ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_END)), phyframes_pfn);                 // don't use the kernel's memory
        phyframes_pfn = MAX(ALIGN_UP_TO_PAGE(platform_info->initrd_pfn + platform_info->initrd_npages), phyframes_pfn); // don't use the initrd's memory

        const pfn_t pfn_end = phyframes_pfn + phyframes_npages;

        pr_info2("using " PFN_RANGE " for the phyframes array", phyframes_pfn, pfn_end);

        // we have to map this into the startup page tables, because we don't have the kernel page tables yet
        mos_startup_map_pages(MOS_PHYFRAME_ARRAY_VADDR, phyframes_pfn << 12, phyframes_npages, VM_RW | VM_GLOBAL);
        // then we zero the array
        memzero((void *) MOS_PHYFRAME_ARRAY_VADDR, phyframes_npages * MOS_PAGE_SIZE);
        // then we can initialize the pmm
        pmm_init_regions(phyframes_count);
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
            continue;
        if (r->reserved)
            pmm_reserve_frames(r->pfn_start, r->nframes);
    }
}
