// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/pmlx/pml1.h"

#include "mos/mm/mm.h"
#include "mos/mm/paging/paging.h"
#include "mos/mm/paging/pml_types.h"

#include <mos_stdlib.h>

#if MOS_PLATFORM_PAGING_LEVELS < 1
#error "Give up your mind"
#endif

void pml1_traverse(pml1_t pml1, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t callback, void *data)
{
    for (size_t pml1_i = pml1_index(*vaddr); pml1_i < PML1_ENTRIES && *n_pages; pml1_i++)
    {
        pml1e_t *pml1e = pml1_entry(pml1, *vaddr);
        if (callback.pml1e_callback)
            callback.pml1e_callback(pml1, pml1e, *vaddr, data);
        (*vaddr) += MIN(*n_pages, PML1E_NPAGES) * MOS_PAGE_SIZE;
        (*n_pages) -= MIN(*n_pages, PML1E_NPAGES);
    }
}

bool pml1_destroy_range(pml1_t pml1, ptr_t *vaddr, size_t *n_pages)
{
    const bool should_zap_this_pml1 = pml1_index(*vaddr) == 0 && *n_pages >= PML1_ENTRIES;

    for (size_t pml1_i = pml1_index(*vaddr); pml1_i < PML1_ENTRIES && *n_pages; pml1_i++)
    {
        pml1e_t *pml1e = pml1_entry(pml1, *vaddr);
        MOS_ASSERT(!pml1e_is_present(pml1e));

        (*vaddr) += PML1E_NPAGES * MOS_PAGE_SIZE;
        (*n_pages) -= PML1E_NPAGES;
    }

    if (should_zap_this_pml1)
        pml_destroy_table(pml1);

    return should_zap_this_pml1;
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
