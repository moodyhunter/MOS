// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/paging/pml_types.h"

void pml1_free(pml1_t pml1);

void pml1_traverse(pml1_t pml1, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t callback, void *data);

__nodiscard bool pml1_destroy_range(pml1_t pml1, ptr_t *vaddr, size_t *n_pages);

pml1e_t *pml1_entry(pml1_t pml1, ptr_t vaddr);

bool pml1e_is_present(const pml1e_t *pml1e);

pfn_t pml1e_get_pfn(const pml1e_t *pml1e);
