// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/platform/platform.h>
#include <mos/types.h>

typedef struct x86_pg_infra_t x86_pg_infra_t;

extern x86_pg_infra_t *const x86_kpg_infra;

void x86_mm_paging_init(void);
void x86_mm_walk_page_table(paging_handle_t handle, ptr_t vaddr_start, size_t n_pages, pgt_iteration_callback_t callback, void *arg);
