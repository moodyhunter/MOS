// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/pmlx/pml2.h"

#include "mos/mm/mm.h"
#include "mos/mm/paging/pmlx/pml1.h"
#include "mos/platform/platform.h"
#include "mos/platform/platform_defs.h"

#include <mos_stdlib.h>
#include <mos_string.h>

void pml2_traverse(pml2_t pml2, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t options, void *data)
{
    for (size_t i = pml2_index(*vaddr); i < PML2_ENTRIES && *n_pages; i++)
    {
        pml2e_t *pml2e = pml2_entry(pml2, *vaddr);
        pml1_t pml1 = { 0 };

        if (pml2e_is_present(pml2e))
        {
            pml1 = pml2e_get_or_create_pml1(pml2e);
        }
        else
        {
            if (options.readonly)
            {
                // skip to the next pml2e, but don't go past the end of the range
                *vaddr += MIN(*n_pages, PML2E_NPAGES) * MOS_PAGE_SIZE;
                *n_pages -= MIN(*n_pages, PML2E_NPAGES);
                continue;
            }

            pml1 = pml_create_table(pml1);
            platform_pml2e_set_pml1(pml2e, pml1, va_pfn(pml1.table));
        }

        if (options.pml2e_pre_traverse)
            options.pml2e_pre_traverse(pml2, pml2e, *vaddr, data);
        pml1_traverse(pml1, vaddr, n_pages, options, data);
    }
}

bool pml2_destroy_range(pml2_t pml2, ptr_t *vaddr, size_t *n_pages)
{
    const bool should_zap_this_pml2 = pml2_index(*vaddr) == 0 && *n_pages >= PML2_ENTRIES * PML2E_NPAGES;

    for (size_t i = pml2_index(*vaddr); i < PML2_ENTRIES && *n_pages; i++)
    {
        pml2e_t *pml2e = pml2_entry(pml2, *vaddr);

        if (pml2e_is_present(pml2e))
        {
            pml1_t pml1 = platform_pml2e_get_pml1(pml2e);
            if (pml1_destroy_range(pml1, vaddr, n_pages))
                pmlxe_destroy(pml2e); // pml1 was destroyed
        }
        else
        {
            // skip to the next pml2e
            *vaddr += MIN(*n_pages, PML2E_NPAGES) * MOS_PAGE_SIZE;
            *n_pages -= MIN(*n_pages, PML2E_NPAGES);
            continue;
        }
    }

    if (should_zap_this_pml2)
        pml_destroy_table(pml2);

    return should_zap_this_pml2;
}

pml2e_t *pml2_entry(pml2_t pml2, ptr_t vaddr)
{
    return &pml2.table[pml2_index(vaddr)];
}

bool pml2e_is_present(const pml2e_t *pml2e)
{
    return platform_pml2e_get_present(pml2e);
}

pml1_t pml2e_get_or_create_pml1(pml2e_t *pml2e)
{
    if (pml2e_is_present(pml2e))
        return platform_pml2e_get_pml1(pml2e);

    pml1_t pml1 = pml_create_table(pml1);
    platform_pml2e_set_pml1(pml2e, pml1, va_pfn(pml1.table));
    return pml1;
}
