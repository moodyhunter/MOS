// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/types.h>

#pragma once

#define MOS_PLATFORM_PAGING_LEVELS 4 // PML4, PDPT, PD, PT

#define PML1_SHIFT   12
#define PML1_MASK    0x1FFL // 9 bits page table offset
#define PML1_ENTRIES 512

#define PML2_SHIFT        21
#define PML2_MASK         0x1FFL // 9 bits page directory offset
#define PML2_ENTRIES      512
#define PML2_HUGE_CAPABLE 1 // 2MB pages

#define PML3_SHIFT        30
#define PML3_MASK         0x1FFL // 9 bits page directory pointer offset
#define PML3_ENTRIES      512
#define PML3_HUGE_CAPABLE 1 // 1GB pages

#define PML4_SHIFT        39
#define PML4_MASK         0x1FFL // 9 bits page map level 4 offset
#define PML4_ENTRIES      512
#define PML4_HUGE_CAPABLE -1
