// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/table_ops/do_copy.h"

#include "mos/mm/paging/pml_types.h"
#include "mos/mm/paging/pmlx/pml1.h"
#include "mos/mm/paging/pmlx/pml2.h"
#include "mos/mm/paging/pmlx/pml3.h"
#include "mos/mm/paging/pmlx/pml4.h"
#include "mos/platform/platform.h"

#include <mos/mos_global.h>
#include <mos_string.h>

static void pml1e_do_copy_callback(pml1_t pml1, pml1e_t *src_e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml1);
    struct pagetable_do_copy_data *copy_data = data;
    copy_data->dest_pml1e = pml1_entry(copy_data->dest_pml1, vaddr);

    const pfn_t old_pfn = platform_pml1e_get_present(copy_data->dest_pml1e) ? platform_pml1e_get_pfn(copy_data->dest_pml1e) : 0;

    if (platform_pml1e_get_present(src_e))
    {
        pmm_ref_one(platform_pml1e_get_pfn(src_e));
        copy_data->dest_pml1e->content = src_e->content;
    }
    else
    {
        pmlxe_destroy(copy_data->dest_pml1e);
    }

    if (old_pfn)
        pmm_unref_one(old_pfn);
}

static void pml2e_do_copy_callback(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml2);
    struct pagetable_do_copy_data *copy_data = data;
    copy_data->dest_pml2e = pml2_entry(copy_data->dest_pml2, vaddr);
    copy_data->dest_pml1 = pml2e_get_or_create_pml1(copy_data->dest_pml2e);

    platform_pml2e_set_flags(copy_data->dest_pml2e, platform_pml2e_get_flags(e));
}

static void pml3e_do_copy_callback(pml3_t pml3, pml3e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml3);
    struct pagetable_do_copy_data *copy_data = data;
    copy_data->dest_pml3e = pml3_entry(copy_data->dest_pml3, vaddr);
    copy_data->dest_pml2 = pml3e_get_or_create_pml2(copy_data->dest_pml3e);

    platform_pml3e_set_flags(copy_data->dest_pml3e, platform_pml3e_get_flags(e));
}

static void pml4e_do_copy_callback(pml4_t pml4, pml4e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml4);
    struct pagetable_do_copy_data *copy_data = data;
    copy_data->dest_pml4e = pml4_entry(copy_data->dest_pml4, vaddr);
    copy_data->dest_pml3 = pml4e_get_or_create_pml3(copy_data->dest_pml4e);

    platform_pml4e_set_flags(copy_data->dest_pml4e, platform_pml4e_get_flags(e));
}

const pagetable_walk_options_t pagetable_do_copy_callbacks = {
    .pml1e_callback = pml1e_do_copy_callback,
    .pml2e_pre_traverse = pml2e_do_copy_callback,
    .pml3e_pre_traverse = pml3e_do_copy_callback,
    .pml4e_pre_traverse = pml4e_do_copy_callback,
};
