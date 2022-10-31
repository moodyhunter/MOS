// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/types.h"

vmblock_t mm_map_proxy_space(paging_handle_t from, uintptr_t fvaddr, size_t npages);
void mm_unmap_proxy_space(vmblock_t proxy_blk);

void mm_copy_pages(paging_handle_t from, uintptr_t src, paging_handle_t to, uintptr_t dst, size_t n_pages);
