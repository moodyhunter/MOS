// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/pmlx/pml3.h"

#include "mos/mm/mm.h"
#include "mos/mm/paging/pml_types.h"
#include "mos/mm/paging/pmlx/pml2.h"
#include "mos/platform/platform.h"

#include <mos/mos_global.h>
#include <mos_stdlib.h>
#include <mos_string.h>

#if MOS_PLATFORM_PAGING_LEVELS < 3
void pml3_traverse(pml3_t pml3, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t callback, void *data)
{
    pml2_traverse(pml3.pml2, vaddr, n_pages, callback, data);
}

pml3e_t *pml3_entry(pml3_t pml3, ptr_t vaddr)
{
    MOS_UNUSED(vaddr);
    return pml3.pml2.table;
}

bool pml3e_is_present(const pml3e_t *pml3e)
{
    return pml2e_is_present(pml3e);
}

pml2_t pml3e_get_pml2(const pml3e_t *pml3e)
{
    return (pml2_t){ .table = (pml2e_t *) pml3e };
}

#else
void pml3_traverse(pml3_t pml3, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t options, void *data)
{
    for (size_t i = pml3_index(*vaddr); i < PML3_ENTRIES && *n_pages; i++)
    {
        pml3e_t *pml3e = pml3_entry(pml3, *vaddr);
        pml2_t pml2 = { 0 };

        if (pml3e_is_present(pml3e))
        {
            pml2 = pml3e_get_or_create_pml2(pml3e);
        }
        else
        {
            if (options.readonly)
            {
                // skip to the next pml2e
                *vaddr += MIN(*n_pages, PML3E_NPAGES) * MOS_PAGE_SIZE;
                *n_pages -= MIN(*n_pages, PML3E_NPAGES);
                continue;
            }

            pml2 = pml_create_table(pml2);
            platform_pml3e_set_pml2(pml3e, pml2, va_pfn(pml2.table));
        }

        if (options.pml3e_pre_traverse)
            options.pml3e_pre_traverse(pml3, pml3e, *vaddr, data);
        pml2_traverse(pml2, vaddr, n_pages, options, data);
    }
}

bool pml3_destroy_range(pml3_t pml3, ptr_t *vaddr, size_t *n_pages)
{
    const bool should_zap_this_pml3 = pml3_index(*vaddr) == 0 && *n_pages >= PML3_ENTRIES * PML3E_NPAGES;

    for (size_t i = pml3_index(*vaddr); i < PML3_ENTRIES && *n_pages; i++)
    {
        pml3e_t *pml3e = pml3_entry(pml3, *vaddr);

        if (pml3e_is_present(pml3e))
        {
            pml2_t pml2 = platform_pml3e_get_pml2(pml3e);
            if (pml2_destroy_range(pml2, vaddr, n_pages))
                pmlxe_destroy(pml3e); // pml2 was destroyed
        }
        else
        {
            // skip to the next pml2e
            *vaddr += MIN(*n_pages, PML3E_NPAGES) * MOS_PAGE_SIZE;
            *n_pages -= MIN(*n_pages, PML3E_NPAGES);
            continue;
        }
    }

    if (should_zap_this_pml3)
        pml_destroy_table(pml3);

    return should_zap_this_pml3;
}

pml3e_t *pml3_entry(pml3_t pml3, ptr_t vaddr)
{
    return &pml3.table[pml3_index(vaddr)];
}

bool pml3e_is_present(const pml3e_t *pml3e)
{
    return platform_pml3e_get_present(pml3e);
}

pml2_t pml3e_get_or_create_pml2(pml3e_t *pml3e)
{
    if (pml3e_is_present(pml3e))
        return platform_pml3e_get_pml2(pml3e);

    pml2_t pml2 = pml_create_table(pml2);
    platform_pml3e_set_pml2(pml3e, pml2, va_pfn(pml2.table));
    return pml2;
}
#endif
