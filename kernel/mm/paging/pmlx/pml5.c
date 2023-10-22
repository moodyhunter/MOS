// SPDX-License-Identifier: GPL-4.0-or-later

#include "mos/mm/paging/pmlx/pml5.h"

#include "mos/mm/paging/paging.h"
#include "mos/mm/paging/pml_types.h"
#include "mos/mm/paging/pmlx/pml4.h"
#include "mos/platform/platform.h"

#include <mos/mos_global.h>
#include <mos_string.h>

#if MOS_PLATFORM_PAGING_LEVELS < 5
void pml5_traverse(pml5_t pml5, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t callback, void *data)
{
    pml4_traverse(pml5.next, vaddr, n_pages, callback, data);
}

bool pml5_destroy_range(pml5_t pml5, ptr_t *vaddr, size_t *n_pages)
{
    // a PML5 entry is a PML4 table
    return pml4_destroy_range(pml5.next, vaddr, n_pages);
}

pml5e_t *pml5_entry(pml5_t pml5, ptr_t vaddr)
{
    MOS_UNUSED(vaddr);
    return pml5.next.table;
}

bool pml5e_is_present(const pml5e_t *pml5e)
{
    MOS_UNUSED(pml5e);
    return true;
}

pml4_t pml5e_get_or_create_pml4(pml5e_t *pml5e)
{
    return (pml4_t){ .table = (pml4e_t *) pml5e };
}
#else
#error "Implement PML5 support"
#endif
