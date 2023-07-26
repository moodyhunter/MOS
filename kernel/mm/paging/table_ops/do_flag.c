// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/table_ops/do_flag.h"

static void pml1_do_flag_callback(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml1);
    MOS_UNUSED(vaddr);
    struct pagetable_do_flag_data *flag_data = data;
    platform_pml1e_set_flags(e, flag_data->flags);
    platform_invalidate_tlb(vaddr);
}

static void pml2_do_flag_callback(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml2);
    MOS_UNUSED(vaddr);
    struct pagetable_do_flag_data *flag_data = data;
    platform_pml2e_set_flags(e, flag_data->flags);
}

static void pml3_do_flag_callback(pml3_t pml3, pml3e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml3);
    MOS_UNUSED(vaddr);
    struct pagetable_do_flag_data *flag_data = data;
    platform_pml3e_set_flags(e, flag_data->flags);
}

static void pml4_do_flag_callback(pml4_t pml4, pml4e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml4);
    MOS_UNUSED(vaddr);
    struct pagetable_do_flag_data *flag_data = data;
    platform_pml4e_set_flags(e, flag_data->flags);
}

const pagetable_walk_options_t pagetable_do_flag_callbacks = {
    .pml1_callback = pml1_do_flag_callback,
    .pml2_callback = pml2_do_flag_callback,
    .pml3_callback = pml3_do_flag_callback,
    .pml4_callback = pml4_do_flag_callback,
};
