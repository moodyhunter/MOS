// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/types.h>

#pragma once

#define MOS_PLATFORM_PAGING_LEVELS 2 // Page table, Page directory

#define MOS_PLATFORM_PML1_SHIFT  12
#define MOS_PLATFORM_PML1_MASK   0x3FF // 10 bits page table offset
#define MOS_PLATFORM_PML1_NPAGES 1024  // 1024 pages

#define MOS_PLATFORM_PML2_SHIFT 22
#define MOS_PLATFORM_PML2_MASK  0x3FF // 10 bits page directory offset
#define MOS_PLATFORM_PML2_NPML1 1024  // 1024 page tables

#define MOS_PLATFORM_HAS_EXTRA_PHYFRAME_INFO 1
struct platform_extra_phyframe_info
{
    // on x86 we can't just map every physical frame to some virtual address,
    // so we need to store the virtual address here
    ptr_t pagetable_vaddr;
};
