// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/pmlx/pml1.h"

#include "mos/mm/memops.h"
#include "mos/mm/paging/paging.h"
#include "mos/mm/paging/pml_types.h"

#if MOS_PLATFORM_PAGING_LEVELS < 1
#error "Give up your mind"
#endif

void pml1_traverse(pml1_t pml1, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t callback, void *data)
{
    for (size_t pml1_i = pml1_index(*vaddr); pml1_i < MOS_PLATFORM_PML1_NPAGES && *n_pages; pml1_i++)
    {
        pml1e_t *pml1e = pml1_entry(pml1, *vaddr);
        callback.pml1_callback(pml1, pml1e, *vaddr, data);
        (*vaddr) += MOS_PAGE_SIZE;
        (*n_pages)--;
    }
}

pml1e_t *pml1_entry(pml1_t pml1, ptr_t vaddr)
{
    return &pml1.table[pml1_index(vaddr)];
}

bool pml1e_is_present(const pml1e_t *pml1e)
{
    return platform_pml1e_get_present(pml1e);
}

pfn_t pml1e_get_pfn(const pml1e_t *pml1e)
{
    return platform_pml1e_get_pfn(pml1e);
}
