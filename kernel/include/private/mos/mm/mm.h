// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/tasks/task_types.h"

vmap_t *mm_new_vmap(vmblock_t block, vmap_content_t content, vmap_flags_t flags);

void mm_attach_vmap(mm_context_t *mmctx, vmap_t *vmap);

void mm_find_vmap(mm_context_t *mmctx, ptr_t vaddr, vmap_t **out_vmap, size_t *out_offset);
