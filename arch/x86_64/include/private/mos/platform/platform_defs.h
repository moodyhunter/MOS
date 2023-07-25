// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/types.h>

#pragma once

#define MOS_PLATFORM_PAGING_LEVELS 4 // PML4, PDPT, PD, PT

#define MOS_PLATFORM_PML1_SHIFT  12
#define MOS_PLATFORM_PML1_MASK   0x1FFL // 9 bits page table offset
#define MOS_PLATFORM_PML1_NPAGES 512

#define MOS_PLATFORM_PML2_SHIFT        21
#define MOS_PLATFORM_PML2_MASK         0x1FFL // 9 bits page directory offset
#define MOS_PLATFORM_PML2_NPML1        512
#define MOS_PLATFORM_PML2_HUGE_CAPABLE 1

#define MOS_PLATFORM_PML3_SHIFT        30
#define MOS_PLATFORM_PML3_MASK         0x1FFL // 9 bits page directory pointer offset
#define MOS_PLATFORM_PML3_NPML2        512
#define MOS_PLATFORM_PML3_HUGE_CAPABLE 1

#define MOS_PLATFORM_PML4_SHIFT        39
#define MOS_PLATFORM_PML4_MASK         0x1FFL // 9 bits page map level 4 offset
#define MOS_PLATFORM_PML4_NPML3        512
#define MOS_PLATFORM_PML4_HUGE_CAPABLE -1 // we don't support 1GiB pages (for now)

#define MOS_PLATFORM_HAS_EXTRA_PHYFRAME_INFO -1
