// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/pmlx/pml3.h"

#include "mos/mm/paging/paging.h"
#include "mos/mm/paging/pml_types.h"
#include "mos/mm/paging/pmlx/pml2.h"
#include "mos/mm/paging/table_ops.h"
#include "mos/platform/platform.h"

#include <mos/kconfig.h>
#include <mos/mos_global.h>
#include <string.h>

#if MOS_PLATFORM_PAGING_LEVELS < 3

void pml3_traverse(pml3_t pml3, ptr_t *vaddr, size_t *n_pages, pagetable_walk_options_t callback, void *data)
{
    pml2_traverse(pml3.pml2, vaddr, n_pages, callback, data);
}

pml3e_t *pml3_entry(pml3_t pml3, ptr_t vaddr)
{
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
#endif
