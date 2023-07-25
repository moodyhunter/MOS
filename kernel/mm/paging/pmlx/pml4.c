// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/pmlx/pml4.h"

#include "mos/mm/memops.h"
#include "mos/mm/paging/paging.h"
#include "mos/mm/paging/pml_types.h"
#include "mos/mm/paging/pmlx/pml3.h"
#include "mos/mm/paging/table_ops.h"
#include "mos/platform/platform.h"

#include <mos/kconfig.h>
#include <mos/mos_global.h>
#include <string.h>

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
    for (size_t i = pml4_index(*vaddr); i < MOS_PLATFORM_PML4_NPML3 && *n_pages; i++)
    {
        pml4e_t *pml4e = pml4_entry(pml4, *vaddr);
        pml3_t pml3 = { 0 };

        if (pml4e_is_present(pml4e))
        {
            pml3 = pml4e_get_pml3(pml4e);
        }
        else
        {
            if (options.readonly)
            {
                // skip to the next pml3e
                *vaddr += MOS_PLATFORM_PML1_NPAGES * MOS_PAGE_SIZE;
                *n_pages -= MOS_PLATFORM_PML1_NPAGES;
                continue;
            }

            const mm_get_one_page_result_t result = mm_get_one_zero_page();
            pml3.table = (void *) result.v;
            platform_pml4e_set_pml3(pml4e, pml3, result.p);
            platform_pml4e_set_present(pml4e, true);
            platform_pml4e_set_flags(pml4e, VM_RW);
        }

        options.pml4_callback(pml4, pml4e, *vaddr, data);
        pml3_traverse(pml3, vaddr, n_pages, options, data);
    }
}

pml4e_t *pml4_entry(pml4_t pml4, ptr_t vaddr)
{
    return &pml4.table[pml4_index(vaddr)];
}

bool pml4e_is_present(const pml4e_t *pml4e)
{
    return platform_pml4e_get_present(pml4e);
}

pml3_t pml4e_get_pml3(const pml4e_t *pml4e)
{
    return platform_pml4e_get_pml3(pml4e);
}
#endif