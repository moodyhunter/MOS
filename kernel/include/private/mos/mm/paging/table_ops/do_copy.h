// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "mos/mm/paging/pml_types.h"

struct pagetable_do_copy_data
{
    pml5_t dest_pml5;

    pml5e_t *dest_pml5e;
    pml4_t dest_pml4;

    pml4e_t *dest_pml4e;
    pml3_t dest_pml3;

    pml3e_t *dest_pml3e;
    pml2_t dest_pml2;

    pml2e_t *dest_pml2e;
    pml1_t dest_pml1;

    pml1e_t *dest_pml1e;
};

extern const pagetable_walk_options_t pagetable_do_copy_callbacks;
