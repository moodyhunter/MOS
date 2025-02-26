// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/paging/pml_types.hpp"
#include "mos/platform/platform.hpp"

void mm_do_map(pgd_t top, ptr_t vaddr, pfn_t pfn, size_t n_pages, VMFlags flags, bool do_refcount);
void mm_do_flag(pgd_t top, ptr_t vaddr, size_t n_pages, VMFlags flags);
void mm_do_unmap(pgd_t top, ptr_t vaddr, size_t n_pages, bool do_unref);
void mm_do_mask_flags(pgd_t max, ptr_t vaddr, size_t n_pages, VMFlags to_remove);
void mm_do_copy(pgd_t src, pgd_t dst, ptr_t vaddr, size_t n_pages);
pfn_t mm_do_get_pfn(pgd_t top, ptr_t vaddr);
VMFlags mm_do_get_flags(pgd_t max, ptr_t vaddr);
bool mm_do_get_present(pgd_t max, ptr_t vaddr);
