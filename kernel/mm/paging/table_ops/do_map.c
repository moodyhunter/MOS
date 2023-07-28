// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/table_ops/do_map.h"

#include "mos/platform/platform.h"

static void pml1e_do_map_callback(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml1);
    MOS_UNUSED(vaddr);
    struct pagetable_do_map_data *map_data = data;
    platform_pml1e_set_present(e, true);
    platform_pml1e_set_flags(e, map_data->flags);
    platform_pml1e_set_pfn(e, map_data->pfn);
    platform_invalidate_tlb(vaddr);
    map_data->pfn++;
}

static void pml2e_do_map_callback(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml2);
    MOS_UNUSED(vaddr);
    struct pagetable_do_map_data *map_data = data;
    platform_pml2e_set_flags(e, map_data->flags);
}

static void pml3e_do_map_callback(pml3_t pml3, pml3e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml3);
    MOS_UNUSED(vaddr);
    struct pagetable_do_map_data *map_data = data;
    platform_pml3e_set_flags(e, map_data->flags);
}

static void pml4e_do_map_callback(pml4_t pml4, pml4e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml4);
    MOS_UNUSED(vaddr);
    struct pagetable_do_map_data *map_data = data;
    platform_pml4e_set_flags(e, map_data->flags);
}

const pagetable_walk_options_t pagetable_do_map_callbacks = {
    .pml1e_callback = pml1e_do_map_callback,
    .pml2e_pre_traverse = pml2e_do_map_callback,
    .pml3e_pre_traverse = pml3e_do_map_callback,
    .pml4e_pre_traverse = pml4e_do_map_callback,
};
