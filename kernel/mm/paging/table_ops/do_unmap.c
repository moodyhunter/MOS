// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/table_ops/do_unmap.h"

#include "mos/platform/platform.h"

static void pml1e_do_unmap_callback(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml1);
    MOS_UNUSED(vaddr);

    struct pagetable_do_unmap_data *unmap_data = data;
    const pfn_t pfn = platform_pml1e_get_pfn(e);
    if (unmap_data->do_unref)
        pmm_unref_frames(pfn, 1);

    platform_pml1e_set_present(e, false);
    platform_invalidate_tlb(vaddr);
}

static void pml2e_do_unmap_callback(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml2);
    MOS_UNUSED(e);
    MOS_UNUSED(vaddr);
    MOS_UNUSED(data);
}

static void pml3e_do_unmap_callback(pml3_t pml3, pml3e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml3);
    MOS_UNUSED(e);
    MOS_UNUSED(vaddr);
    MOS_UNUSED(data);
}

static void pml4e_do_unmap_callback(pml4_t pml4, pml4e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml4);
    MOS_UNUSED(e);
    MOS_UNUSED(vaddr);
    MOS_UNUSED(data);
}

const pagetable_walk_options_t pagetable_do_unmap_callbacks = {
    .pml1e_callback = pml1e_do_unmap_callback,
    .pml2e_pre_traverse = pml2e_do_unmap_callback,
    .pml3e_pre_traverse = pml3e_do_unmap_callback,
    .pml4e_pre_traverse = pml4e_do_unmap_callback,
};
