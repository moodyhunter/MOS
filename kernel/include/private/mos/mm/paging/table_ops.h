// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/paging/pml_types.h"
#include "mos/platform/platform.h"
#include "mos/platform/platform_defs.h"

#include <mos/mos_global.h>

void mm_do_map(pmlmax_t top, ptr_t vaddr, pfn_t pfn, size_t n_pages, vm_flags flags);
void mm_do_flag(pmlmax_t top, ptr_t vaddr, size_t n_pages, vm_flags flags);
void mm_do_unmap(pmlmax_t top, ptr_t vaddr, size_t n_pages);

pfn_t mm_do_get_pfn(pmlmax_t top, ptr_t vaddr);

pfn_t mm_pmlmax_get_pfn(pmlmax_t top);

pmlmax_t mm_pmlmax_create(pmltop_t top, pfn_t pfn);
