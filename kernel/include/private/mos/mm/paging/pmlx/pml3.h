// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/paging/pml_types.h"

void pml3_traverse(pml3_t pml3, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t callback, void *data);

pml3e_t *pml3_entry(pml3_t pml3, ptr_t vaddr);

bool pml3e_is_present(const pml3e_t *pml3e);

pml2_t pml3e_get_pml2(const pml3e_t *pml3e);
