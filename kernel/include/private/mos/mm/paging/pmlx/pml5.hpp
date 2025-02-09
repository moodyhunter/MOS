// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/paging/pml_types.hpp"

void pml5_traverse(pml5_t pml5, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t callback, void *data);

__nodiscard bool pml5_destroy_range(pml5_t pml5, ptr_t *vaddr, size_t *n_pages);

pml5e_t *pml5_entry(pml5_t pml5, ptr_t vaddr);

bool pml5e_is_present(const pml5e_t *pml5e);

pml4_t pml5e_get_or_create_pml4(pml5e_t *pml5e);
