// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/table_ops.h"

#include "mos/mm/paging/pml_types.h"
#include "mos/mm/paging/pmlx/pml1.h"
#include "mos/mm/paging/pmlx/pml2.h"
#include "mos/mm/paging/pmlx/pml3.h"
#include "mos/printk.h"

struct do_map_data
{
    pfn_t pfn;
    vm_flags flags;
};

static void pml1_do_map_callback(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data)
{
    struct do_map_data *map_data = data;
    platform_pml1e_set_present(e, true);
    platform_pml1e_set_flags(e, map_data->flags);
    platform_pml1e_set_pfn(e, map_data->pfn);
    map_data->pfn++;
}

static void pml2_do_map_callback(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data)
{
    struct do_map_data *map_data = data;
    platform_pml2e_set_flags(e, map_data->flags);
}

static const pagetable_walk_options_t map_callbacks = {
    .pml1_callback = pml1_do_map_callback,
    .pml2_callback = pml2_do_map_callback,
};

void mm_do_map(pmlmax_t max, ptr_t vaddr, pfn_t pfn, size_t n_pages, vm_flags flags)
{
    pr_emph("NEWAPI: mm_do_map(" PTR_FMT ", " PFN_FMT ", %zu, 0x%x)", vaddr, pfn, n_pages, flags);
    struct do_map_data data = { .pfn = pfn, .flags = flags };
    pml3_traverse(max.real_max, &vaddr, &n_pages, map_callbacks, &data);
}

struct do_flag_data
{
    vm_flags flags;
};

static void pml1_do_flag_callback(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data)
{
    struct do_flag_data *flag_data = data;
    platform_pml1e_set_flags(e, flag_data->flags);
}

static void pml2_do_flag_callback(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data)
{
    struct do_flag_data *flag_data = data;
    platform_pml2e_set_flags(e, flag_data->flags);
}

static const pagetable_walk_options_t flag_callbacks = {
    .pml1_callback = pml1_do_flag_callback,
    .pml2_callback = pml2_do_flag_callback,
};

void mm_do_flag(pmlmax_t max, ptr_t vaddr, size_t n_pages, vm_flags flags)
{
    pr_emph("NEWAPI: mm_do_flag(" PTR_FMT ", %zu, 0x%x)", vaddr, n_pages, flags);
    struct do_flag_data data = { .flags = flags };
    pml3_traverse(max.real_max, &vaddr, &n_pages, flag_callbacks, &data);
}

static void pml1_do_unmap_callback(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data)
{
    platform_pml1e_set_present(e, false);
}

static void pml2_do_unmap_callback(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data)
{
    platform_pml2e_set_present(e, false);
}

static const pagetable_walk_options_t unmap_callbacks = {
    .pml1_callback = pml1_do_unmap_callback,
    .pml2_callback = pml2_do_unmap_callback,
};

void mm_do_unmap(pmlmax_t max, ptr_t vaddr, size_t n_pages)
{
    pr_emph("NEWAPI: mm_do_unmap(" PTR_FMT ", %zu)", vaddr, n_pages);
    pml3_traverse(max.real_max, &vaddr, &n_pages, unmap_callbacks, NULL);
}

pfn_t mm_do_get_pfn(pmlmax_t max, ptr_t vaddr)
{
    const pml3e_t *pml3e = pml3_entry(max.real_max, vaddr);
    if (!pml3e_is_present(pml3e))
    {
        pr_emph("NEWAPI: mm_do_get_pfn(" PTR_FMT ") = 0", vaddr);
        return 0;
    }

    const pml2_t pml2 = pml3e_get_pml2(pml3e);
    const pml2e_t *pml2e = pml2_entry(pml2, vaddr);
    if (!pml2e_is_present(pml2e))
    {
        pr_emph("NEWAPI: mm_do_get_pfn(" PTR_FMT ") = 0", vaddr);
        return 0;
    }

    const pml1_t pml1 = pml2e_get_pml1(pml2e);
    const pml1e_t *pml1e = pml1_entry(pml1, vaddr);
    if (!pml1e_is_present(pml1e))
    {
        pr_emph("NEWAPI: mm_do_get_pfn(" PTR_FMT ") = 0", vaddr);
        return 0;
    }

    const pfn_t pfn = pml1e_get_pfn(pml1e);
    pr_emph("NEWAPI: mm_do_get_pfn(" PTR_FMT ") = " PFN_FMT, vaddr, pfn);
    return pfn;
}

pfn_t mm_pmlmax_get_pfn(pmlmax_t max)
{
    return max.pfn;
}

pmlmax_t mm_pmlmax_create(pmltop_t top, pfn_t pfn)
{
    pmlmax_t max = { .platform_top = top, .pfn = pfn, .real_max = { .pml2 = top } };
    return max;
}
