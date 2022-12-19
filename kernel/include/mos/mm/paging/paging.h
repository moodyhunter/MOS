// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/bitmap.h"
#include "mos/platform/platform.h"

#define MOS_PAGEMAP_MAX_LINES    BITMAP_LINE_COUNT((uintptr_t) ~0 / MOS_PAGE_SIZE)
#define MOS_PAGEMAP_USER_LINES   BITMAP_LINE_COUNT(MOS_KERNEL_START_VADDR / MOS_PAGE_SIZE)
#define MOS_PAGEMAP_KERNEL_LINES (MOS_PAGEMAP_MAX_LINES - MOS_PAGEMAP_USER_LINES)

typedef struct _page_map
{
    bitmap_line_t ummap[MOS_PAGEMAP_USER_LINES];
} page_map_t;

vmblock_t mm_get_free_pages(paging_handle_t table, size_t npages, pgalloc_hints hints);

vmblock_t mm_alloc_pages(paging_handle_t table, size_t npages, pgalloc_hints hints, vm_flags flags);
vmblock_t mm_alloc_pages_at(paging_handle_t table, uintptr_t vaddr, size_t npages, vm_flags flags);
void mm_free_pages(paging_handle_t table, vmblock_t block);

void mm_map_pages(paging_handle_t table, vmblock_t block);
void mm_map_allocated_pages(paging_handle_t table, vmblock_t block);
void mm_unmap_pages(paging_handle_t table, uintptr_t vaddr, size_t npages);

vmblock_t mm_copy_maps(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages);
bool mm_get_is_mapped(paging_handle_t table, uintptr_t vaddr);

paging_handle_t mm_create_user_pgd();
void mm_destroy_user_pgd(paging_handle_t table);
