// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.hpp"

typedef struct
{
    bool present;
    ptr_t vaddr, vaddr_end;
    pfn_t pfn, pfn_end;
    VMFlags flags;
} pagetable_iter_range_t;

typedef struct
{
    pgd_t pgd;

    ptr_t start;
    ptr_t end;
    ptr_t vaddr;

    pagetable_iter_range_t range;
} pagetable_iter_t;

/**
 * @brief Initialize a pagetable iterator.
 *
 * @param it The iterator to initialize.
 * @param pgd The page directory to iterate.
 * @param vaddr The virtual address to start iterating from.
 * @param end The virtual address to stop iterating at.
 */
void pagetable_iter_init(pagetable_iter_t *it, pgd_t pgd, ptr_t vaddr, ptr_t end);

/**
 * @brief Get the next page table range.
 *
 * @param it The iterator to get the next range from.
 * @return pagetable_iter_range_t*
 */
pagetable_iter_range_t *pagetable_iter_next(pagetable_iter_t *it);
