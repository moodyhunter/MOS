// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/paging/paging.h"

#include <mos/platform/platform.h>

void kpagemap_mark_used(ptr_t vaddr, size_t n_pages);
void kpagemap_mark_free(ptr_t vaddr, size_t n_pages);
ptr_t kpagemap_get_free_pages(size_t n_pages, ptr_t base_vaddr, valloc_flags flags);
bool kpagemap_get_mapped(ptr_t vaddr);
