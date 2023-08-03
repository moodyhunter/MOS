// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mmstat.h"
#include "mos/tasks/task_types.h"

phyframe_t *mm_get_free_page(mem_type_t type);
phyframe_t *mm_get_free_page_raw(mem_type_t type);
phyframe_t *mm_get_free_pages(size_t npages, mem_type_t type);

vmap_t *mm_new_vmap(vmblock_t block, vmap_content_t content, vmap_flags_t flags);
void mm_attach_vmap(mm_context_t *mmctx, vmap_t *vmap);
void mm_find_vmap(mm_context_t *mmctx, ptr_t vaddr, vmap_t **out_vmap, size_t *out_offset);
