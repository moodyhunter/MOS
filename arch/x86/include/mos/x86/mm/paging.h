// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/types.h"

typedef struct x86_pg_infra_t x86_pg_infra_t;

extern x86_pg_infra_t *x86_kpg_infra;

void x86_mm_prepare_paging();
void x86_mm_enable_paging(void);
void x86_mm_dump_page_table(x86_pg_infra_t *pg);

paging_handle_t x86_um_pgd_create();
void x86_um_pgd_destroy(paging_handle_t pgt);

vmblock_t x86_mm_pg_alloc(paging_handle_t pgt, size_t n, pgalloc_hints flags, vm_flags vm_flags);
vmblock_t x86_mm_pg_alloc_at(paging_handle_t pgt, uintptr_t addr, size_t n, vm_flags vm_flags);
void x86_mm_pg_free(paging_handle_t pgt, uintptr_t vaddr, size_t n);
void x86_mm_pg_flag(paging_handle_t pgt, uintptr_t vaddr, size_t n, vm_flags flags);

vmblock_t x86_mm_copy_maps(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages);
void x86_mm_pg_unmap(paging_handle_t table, uintptr_t vaddr, size_t n);
