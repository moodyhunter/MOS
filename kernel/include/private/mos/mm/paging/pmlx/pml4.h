// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/paging/pml_types.h"

void pml4_traverse(pml4_t pml4, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t callback, void *data);

__nodiscard bool pml4_destroy_range(pml4_t pml4, ptr_t *vaddr, size_t *n_pages);

pml4e_t *pml4_entry(pml4_t pml4, ptr_t vaddr);

bool pml4e_is_present(const pml4e_t *pml4e);

pml3_t pml4e_get_or_create_pml3(pml4e_t *pml4e);
