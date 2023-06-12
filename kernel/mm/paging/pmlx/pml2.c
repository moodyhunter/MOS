// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/pmlx/pml2.h"

#include "mos/mm/memops.h"
#include "mos/mm/paging/paging.h"
#include "mos/mm/paging/pmlx/pml1.h"

#include <string.h>

void pml2_traverse(pml2_t pml2, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t options, void *data)
{
    for (size_t i = pml2_index(*vaddr); i < MOS_PLATFORM_PML2_NPML1 && *n_pages; i++)
    {
        pml2e_t *pml2e = pml2_entry(pml2, *vaddr);
        pml1_t pml1 = { 0 };

        if (pml2e_is_present(pml2e))
        {
            pml1 = pml2e_get_pml1(pml2e);
        }
        else
        {
            if (options.readonly)
            {
                // skip to the next pml2e
                *vaddr += MOS_PLATFORM_PML1_NPAGES * MOS_PAGE_SIZE;
                *n_pages -= MOS_PLATFORM_PML1_NPAGES;
                continue;
            }

            const mm_get_one_page_result_t result = mm_get_one_zero_page();
            pml1.table = (void *) result.v;
            platform_pml2e_set_pml1(pml2e, pml1, result.p);
            platform_pml2e_set_present(pml2e, true);
            platform_pml2e_set_flags(pml2e, VM_RW);
        }

        options.pml2_callback(pml2, pml2e, *vaddr, data);
        pml1_traverse(pml1, vaddr, n_pages, options, data);
    }
}

pml2e_t *pml2_entry(pml2_t pml2, ptr_t vaddr)
{
    return &pml2.table[pml2_index(vaddr)];
}

bool pml2e_is_present(const pml2e_t *pml2e)
{
    return platform_pml2e_get_present(pml2e);
}

pml1_t pml2e_get_pml1(const pml2e_t *pml2e)
{
    return platform_pml2e_get_pml1(pml2e);
}
