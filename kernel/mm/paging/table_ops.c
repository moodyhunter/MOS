// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/table_ops.h"

#include "mos/mm/paging/pml_types.h"
#include "mos/mm/paging/pmlx/pml1.h"
#include "mos/mm/paging/pmlx/pml2.h"
#include "mos/mm/paging/pmlx/pml3.h"
#include "mos/mm/paging/pmlx/pml4.h"
#include "mos/mm/paging/pmlx/pml5.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

#include <mos/types.h>

struct do_map_data
{
    pfn_t pfn;
    vm_flags flags;
};

static void pml1_do_map_callback(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml1);
    MOS_UNUSED(vaddr);
    struct do_map_data *map_data = data;
    platform_pml1e_set_present(e, true);
    platform_pml1e_set_flags(e, map_data->flags);
    platform_pml1e_set_pfn(e, map_data->pfn);
    platform_invalidate_tlb(vaddr);
    map_data->pfn++;
}

static void pml2_do_map_callback(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml2);
    MOS_UNUSED(vaddr);
    struct do_map_data *map_data = data;
    platform_pml2e_set_flags(e, map_data->flags);
}

static void pml3_do_map_callback(pml3_t pml3, pml3e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml3);
    MOS_UNUSED(vaddr);
    struct do_map_data *map_data = data;
    platform_pml3e_set_flags(e, map_data->flags);
}

static void pml4_do_map_callback(pml4_t pml4, pml4e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml4);
    MOS_UNUSED(vaddr);
    struct do_map_data *map_data = data;
    platform_pml4e_set_flags(e, map_data->flags);
}

static const pagetable_walk_options_t map_callbacks = {
    .pml1_callback = pml1_do_map_callback,
    .pml2_callback = pml2_do_map_callback,
    .pml3_callback = pml3_do_map_callback,
    .pml4_callback = pml4_do_map_callback,
};

void mm_do_map(pgd_t max, ptr_t vaddr, pfn_t pfn, size_t n_pages, vm_flags flags)
{
    pr_emph("NEWAPI: mm_do_map(" PTR_FMT ", " PTR_FMT ", " PFN_FMT ", %zu, 0x%x)", (ptr_t) max.max.pml4.table, vaddr, pfn, n_pages, flags);
    struct do_map_data data = { .pfn = pfn, .flags = flags };
    pml5_traverse(max.max, &vaddr, &n_pages, map_callbacks, &data);
}

struct do_flag_data
{
    vm_flags flags;
};

static void pml1_do_flag_callback(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml1);
    MOS_UNUSED(vaddr);
    struct do_flag_data *flag_data = data;
    platform_pml1e_set_flags(e, flag_data->flags);
    platform_invalidate_tlb(vaddr);
}

static void pml2_do_flag_callback(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml2);
    MOS_UNUSED(vaddr);
    struct do_flag_data *flag_data = data;
    platform_pml2e_set_flags(e, flag_data->flags);
}

static const pagetable_walk_options_t flag_callbacks = {
    .pml1_callback = pml1_do_flag_callback,
    .pml2_callback = pml2_do_flag_callback,
};

void mm_do_flag(pgd_t max, ptr_t vaddr, size_t n_pages, vm_flags flags)
{
    pr_emph("NEWAPI: mm_do_flag(" PTR_FMT ", %zu, 0x%x)", vaddr, n_pages, flags);
    struct do_flag_data data = { .flags = flags };
    pml5_traverse(max.max, &vaddr, &n_pages, flag_callbacks, &data);
}

static void pml1_do_unmap_callback(pml1_t pml1, pml1e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml1);
    MOS_UNUSED(vaddr);
    MOS_UNUSED(data);
    platform_pml1e_set_present(e, false);
    platform_invalidate_tlb(vaddr);
}

static void pml2_do_unmap_callback(pml2_t pml2, pml2e_t *e, ptr_t vaddr, void *data)
{
    MOS_UNUSED(pml2);
    MOS_UNUSED(vaddr);
    MOS_UNUSED(data);
    platform_pml2e_set_present(e, false);
}

static const pagetable_walk_options_t unmap_callbacks = {
    .pml1_callback = pml1_do_unmap_callback,
    .pml2_callback = pml2_do_unmap_callback,
};

void mm_do_unmap(pgd_t max, ptr_t vaddr, size_t n_pages)
{
    pr_emph("NEWAPI: mm_do_unmap(" PTR_FMT ", %zu)", vaddr, n_pages);
    pml5_traverse(max.max, &vaddr, &n_pages, unmap_callbacks, NULL);
}

pfn_t mm_do_get_pfn(pgd_t max, ptr_t vaddr)
{
    vaddr = ALIGN_DOWN_TO_PAGE(vaddr);
    pml5e_t *pml5e = pml5_entry(max.max, vaddr);
    if (!pml5e_is_present(pml5e))
    {
        pr_emph("NEWAPI: mm_do_get_pfn(" PTR_FMT ") = 0", vaddr);
        return 0;
    }

    const pml4_t pml4 = pml5e_get_pml4(pml5e);
    pml4e_t *pml4e = pml4_entry(pml4, vaddr);
    if (!pml4e_is_present(pml4e))
    {
        pr_emph("NEWAPI: mm_do_get_pfn(" PTR_FMT ") = 0", vaddr);
        return 0;
    }

#if MOS_CONFIG(MOS_PLATFORM_PML4_HUGE_CAPABLE)
    if (platform_pml4e_is_huge(pml4e))
        return platform_pml4e_get_huge_pfn(pml4e) + (vaddr & PML4_HUGE_MASK) / MOS_PAGE_SIZE;
#endif

    const pml3_t pml3 = pml4e_get_pml3(pml4e);
    pml3e_t *pml3e = pml3_entry(pml3, vaddr);
    if (!pml3e_is_present(pml3e))
    {
        pr_emph("NEWAPI: mm_do_get_pfn(" PTR_FMT ") = 0", vaddr);
        return 0;
    }

#if MOS_CONFIG(MOS_PLATFORM_PML3_HUGE_CAPABLE)
    if (platform_pml3e_is_huge(pml3e))
        return platform_pml3e_get_huge_pfn(pml3e) + (vaddr & PML3_HUGE_MASK) / MOS_PAGE_SIZE;
#endif

    const pml2_t pml2 = pml3e_get_pml2(pml3e);
    pml2e_t *pml2e = pml2_entry(pml2, vaddr);
    if (!pml2e_is_present(pml2e))
    {
        pr_emph("NEWAPI: mm_do_get_pfn(" PTR_FMT ") = 0", vaddr);
        return 0;
    }

#if MOS_CONFIG(MOS_PLATFORM_PML2_HUGE_CAPABLE)
    if (platform_pml2e_is_huge(pml2e))
        return platform_pml2e_get_huge_pfn(pml2e) + (vaddr & PML2_HUGE_MASK) / MOS_PAGE_SIZE;
#endif

    const pml1_t pml1 = pml2e_get_pml1(pml2e);
    const pml1e_t *pml1e = pml1_entry(pml1, vaddr);
    if (!pml1e_is_present(pml1e))
    {
        pr_emerg("NEWAPI: mm_do_get_pfn(" PTR_FMT ") = 0", vaddr);
        return 0;
    }

    const pfn_t pfn = pml1e_get_pfn(pml1e);
    pr_emph("NEWAPI: mm_do_get_pfn(" PTR_FMT ") = " PFN_FMT, vaddr, pfn);
    return pfn;
}

pfn_t mm_pmlmax_get_pfn(pgd_t max)
{
    return va_pfn(max.max.pml4.table);
}

pgd_t mm_pgd_create(pmltop_t top)
{
    pgd_t pgd = { .max = { .pml4 = top } };
    return pgd;
}
