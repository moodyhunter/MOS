// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/paging/pml_types.h"

void pml2_traverse(pml2_t pml2, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t callback, void *data);

pml2e_t *pml2_entry(pml2_t pml2, ptr_t vaddr);

bool pml2e_is_present(const pml2e_t *pml2e);

pml1_t pml2e_get_pml1(const pml2e_t *pml2e);
