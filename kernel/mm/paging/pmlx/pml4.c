// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/pmlx/pml4.h"

#include "mos/mm/mm.h"
#include "mos/mm/paging/paging.h"
#include "mos/mm/paging/pml_types.h"
#include "mos/mm/paging/pmlx/pml3.h"
#include "mos/mm/physical/pmm.h"
#include "mos/platform/platform.h"
#include "mos/platform/platform_defs.h"

#include <mos/mos_global.h>
#include <mos_stdlib.h>
#include <mos_string.h>

#if MOS_PLATFORM_PAGING_LEVELS < 4
void pml4_traverse(pml4_t pml4, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t callback, void *data)
{
    pml3_traverse(pml4.pml3, vaddr, n_pages, callback, data);
}

pml4e_t *pml4_entry(pml4_t pml4, ptr_t vaddr)
{
    MOS_UNUSED(vaddr);
    return pml4.pml3.table;
}

bool pml4e_is_present(const pml4e_t *pml4e)
{
    return pml3e_is_present(pml4e);
}

pml3_t pml4e_get_pml3(const pml4e_t *pml4e)
{
    return (pml3_t){ .table = (pml3e_t *) pml4e };
}

#else
void pml4_traverse(pml4_t pml4, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t options, void *data)
{
    for (size_t i = pml4_index(*vaddr); i < PML4_ENTRIES && *n_pages; i++)
    {
        pml4e_t *pml4e = pml4_entry(pml4, *vaddr);
        pml3_t pml3 = { 0 };

        if (pml4e_is_present(pml4e))
        {
            pml3 = pml4e_get_or_create_pml3(pml4e);
        }
        else
        {
            if (options.readonly)
            {
                // skip to the next pml3e
                *vaddr += MIN(*n_pages, PML4E_NPAGES) * MOS_PAGE_SIZE;
                *n_pages -= MIN(*n_pages, PML4E_NPAGES);
                continue;
            }

            pml3 = pml_create_table(pml3);
            platform_pml4e_set_present(pml4e, true);
            platform_pml4e_set_pml3(pml4e, pml3, va_pfn(pml3.table));
        }

        if (options.pml4e_pre_traverse)
            options.pml4e_pre_traverse(pml4, pml4e, *vaddr, data);
        pml3_traverse(pml3, vaddr, n_pages, options, data);
    }
}

bool pml4_destroy_range(pml4_t pml4, ptr_t *vaddr, size_t *n_pages)
{
#if MOS_PLATFORM_PAGING_LEVELS <= 4
    const bool should_zap_this_pml4 = pml4_index(*vaddr) == 0 && *n_pages == (MOS_USER_END_VADDR + 1) / MOS_PAGE_SIZE;
#else
    const bool should_zap_this_pml4 = pml4_index(*vaddr) == 0 && *n_pages >= PML4_ENTRIES * PML4E_NPAGES;
#endif

    for (size_t i = pml4_index(*vaddr); i < PML4_ENTRIES && *n_pages; i++)
    {
        pml4e_t *pml4e = pml4_entry(pml4, *vaddr);

        if (pml4e_is_present(pml4e))
        {
            pml3_t pml3 = platform_pml4e_get_pml3(pml4e);
            if (pml3_destroy_range(pml3, vaddr, n_pages))
                platform_pml4e_set_present(pml4e, false); // pml3 was destroyed
        }
        else
        {
            // skip to the next pml3e
            *vaddr += MIN(*n_pages, PML4E_NPAGES) * MOS_PAGE_SIZE;
            *n_pages -= MIN(*n_pages, PML4E_NPAGES);
            continue;
        }
    }

    if (should_zap_this_pml4)
        pml_destroy_table(pml4);

    return should_zap_this_pml4;
}

pml4e_t *pml4_entry(pml4_t pml4, ptr_t vaddr)
{
    return &pml4.table[pml4_index(vaddr)];
}

bool pml4e_is_present(const pml4e_t *pml4e)
{
    return platform_pml4e_get_present(pml4e);
}

pml3_t pml4e_get_or_create_pml3(pml4e_t *pml4e)
{
    if (pml4e_is_present(pml4e))
        return platform_pml4e_get_pml3(pml4e);

    pml3_t pml3 = pml_create_table(pml3);
    platform_pml4e_set_present(pml4e, true);
    platform_pml4e_set_pml3(pml4e, pml3, va_pfn(pml3.table));
    return pml3;
}
#endif
