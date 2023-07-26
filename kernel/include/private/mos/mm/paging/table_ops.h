// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/paging/pml_types.h"
#include "mos/platform/platform.h"
#include "mos/platform/platform_defs.h"

#include <mos/mos_global.h>

void mm_do_map(pgd_t top, ptr_t vaddr, pfn_t pfn, size_t n_pages, vm_flags flags);
void mm_do_flag(pgd_t top, ptr_t vaddr, size_t n_pages, vm_flags flags);
void mm_do_unmap(pgd_t top, ptr_t vaddr, size_t n_pages);

pfn_t mm_do_get_pfn(pgd_t top, ptr_t vaddr);

#define pfn_va(pfn)        (platform_info->direct_map_base + (pfn) *MOS_PAGE_SIZE)
#define va_pfn(va)         ((((ptr_t) (va)) - platform_info->direct_map_base) / MOS_PAGE_SIZE)
#define va_phyframe(va)    (phyframes + va_pfn(va))
#define phyframe_va(frame) pfn_va(phyframe_pfn(frame))

pfn_t pgd_pfn(pgd_t pgd)
{
    return va_pfn(pgd.max.MOS_PMLTOP_TYPE.table);
}

pgd_t pgd_from_pmltop(pmltop_t top)
{
    return (pgd_t){
        .max = { .MOS_PMLTOP_TYPE = top },
    };
}
