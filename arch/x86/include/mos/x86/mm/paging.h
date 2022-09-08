// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mm_types.h"
#include "mos/platform/platform.h"
#include "mos/types.h"

typedef struct x86_pg_infra_t x86_pg_infra_t;

extern x86_pg_infra_t *x86_kpg_infra;

void x86_mm_prepare_paging();
void x86_mm_enable_paging(x86_pg_infra_t *kpg_infra);
void x86_mm_dump_page_table(x86_pg_infra_t *pg);

paging_handle_t x86_um_pgd_create();
void x86_um_pgd_destroy(paging_handle_t pgt);

void *x86_mm_pg_alloc(paging_handle_t pgt, size_t n, pagealloc_flags flags);
bool x86_mm_pg_free(paging_handle_t pgt, uintptr_t vaddr, size_t n);
void x86_mm_pg_flag(paging_handle_t pgt, uintptr_t vaddr, size_t n, vm_flags flags);

void x86_mm_pg_map_to_kvirt(paging_handle_t table, uintptr_t vaddr, uintptr_t kvaddr, size_t n, vm_flags flags);
void x86_mm_pg_unmap(paging_handle_t table, uintptr_t vaddr, size_t n);
