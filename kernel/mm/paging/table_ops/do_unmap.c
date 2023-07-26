// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/table_ops/do_unmap.h"

#include "mos/platform/platform.h"

static void pml1_do_unmap_callback(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml1);
    MOS_UNUSED(vaddr);
    MOS_UNUSED(data);
    pfn_t pfn = platform_pml1e_get_pfn(e);
    platform_pml1e_set_present(e, false);
    platform_invalidate_tlb(vaddr);
    pmm_unref_frames(pfn, 1);
}

static void pml2_do_unmap_callback(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml2);
    MOS_UNUSED(vaddr);
    MOS_UNUSED(data);
    // platform_pml2e_set_present(e, false);
}

static void pml3_do_unmap_callback(pml3_t pml3, pml3e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml3);
    MOS_UNUSED(vaddr);
    MOS_UNUSED(data);
    // platform_pml3e_set_present(e, false);
}

static void pml4_do_unmap_callback(pml4_t pml4, pml4e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml4);
    MOS_UNUSED(vaddr);
    MOS_UNUSED(data);
    // platform_pml4e_set_present(e, false);
}

const pagetable_walk_options_t pagetable_do_unmap_callbacks = {
    .pml1_callback = pml1_do_unmap_callback,
    .pml2_callback = pml2_do_unmap_callback,
    .pml3_callback = pml3_do_unmap_callback,
    .pml4_callback = pml4_do_unmap_callback,
};
