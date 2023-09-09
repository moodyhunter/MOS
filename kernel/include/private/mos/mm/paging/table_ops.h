// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/paging/pml_types.h"
#include "mos/platform/platform.h"

void mm_do_map(pgd_t top, ptr_t vaddr, pfn_t pfn, size_t n_pages, vm_flags flags, bool do_refcount);
void mm_do_flag(pgd_t top, ptr_t vaddr, size_t n_pages, vm_flags flags);
void mm_do_unmap(pgd_t top, ptr_t vaddr, size_t n_pages, bool do_unref);
void mm_do_mask_flags(pgd_t max, ptr_t vaddr, size_t n_pages, vm_flags to_remove);
void mm_do_copy(pgd_t src, pgd_t dst, ptr_t vaddr, size_t n_pages);
pfn_t mm_do_get_pfn(pgd_t top, ptr_t vaddr);
