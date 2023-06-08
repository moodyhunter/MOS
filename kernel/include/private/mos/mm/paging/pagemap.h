// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/paging/paging.h"

#include <mos/platform/platform.h>

#define IS_KHEAP_VADDR(vaddr, n_pages) (vaddr >= MOS_ADDR_KERNEL_HEAP && vaddr + n_pages * MOS_PAGE_SIZE - 1 <= MOS_ADDR_KERNEL_HEAP_END)

void kpagemap_mark_used(ptr_t vaddr, size_t n_pages);
void kpagemap_mark_free(ptr_t vaddr, size_t n_pages);
ptr_t kpagemap_get_free_pages(size_t n_pages, ptr_t base_vaddr, valloc_flags flags);
bool kpagemap_get_mapped(ptr_t vaddr);
