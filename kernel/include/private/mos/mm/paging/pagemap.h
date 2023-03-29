// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/platform/platform.h>

void pagemap_mark_used(page_map_t *map, ptr_t vaddr, size_t n_pages);
void pagemap_mark_free(page_map_t *map, ptr_t vaddr, size_t n_pages);

bool pagemap_get_mapped(page_map_t *map, ptr_t vaddr);
