// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/table_ops/do_mask.hpp"

#include "mos/platform/platform.hpp"

static void pml1e_do_mask_callback(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml1);
    MOS_UNUSED(vaddr);
    if (platform_pml1e_get_present(e))
    {
        struct pagetable_do_mask_data *mask_data = (pagetable_do_mask_data *) data;
        VMFlags flags = platform_pml1e_get_flags(e);
        flags.erase(mask_data->mask);
        platform_pml1e_set_flags(e, flags);
        platform_invalidate_tlb(vaddr);
    }
}

const pagetable_walk_options_t pagetable_do_mask_callbacks = {
    .pml1e_callback = pml1e_do_mask_callback,
};
