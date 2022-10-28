// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/types.h"

void copy_cow_pages_inplace(uintptr_t vaddr, size_t npages);
vmblock_t mm_make_process_map_cow(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages);
bool cow_handle_page_fault(uintptr_t fault_addr);
