// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/table_ops.h"

#include "mos/mm/mm.h"
#include "mos/mm/mmstat.h"
#include "mos/mm/paging/pml_types.h"
#include "mos/mm/paging/pmlx/pml1.h"
#include "mos/mm/paging/pmlx/pml2.h"
#include "mos/mm/paging/pmlx/pml3.h"
#include "mos/mm/paging/pmlx/pml4.h"
#include "mos/mm/paging/pmlx/pml5.h"
#include "mos/mm/paging/table_ops/do_copy.h"
#include "mos/mm/paging/table_ops/do_flag.h"
#include "mos/mm/paging/table_ops/do_map.h"
#include "mos/mm/paging/table_ops/do_mask.h"
#include "mos/mm/paging/table_ops/do_unmap.h"
#include "mos/mm/physical/pmm.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

#include <mos/types.h>

void mm_do_map(pgd_t pgd, ptr_t vaddr, pfn_t pfn, size_t n_pages, vm_flags flags, bool do_refcount)
{
    struct pagetable_do_map_data data = { .pfn = pfn, .flags = flags, .do_refcount = do_refcount };
    pml5_traverse(pgd.max, &vaddr, &n_pages, pagetable_do_map_callbacks, &data);
}

void mm_do_flag(pgd_t max, ptr_t vaddr, size_t n_pages, vm_flags flags)
{
    struct pagetable_do_flag_data data = { .flags = flags };
    pml5_traverse(max.max, &vaddr, &n_pages, pagetable_do_flag_callbacks, &data);
}

void mm_do_unmap(pgd_t max, ptr_t vaddr, size_t n_pages, bool do_unref)
{
    mos_debug(vmm, "mm_do_unmap: vaddr=" PTR_FMT ", n_pages=%zu, do_unref=%d", vaddr, n_pages, do_unref);
    ptr_t vaddr1 = vaddr;
    size_t n_pages1 = n_pages;

    struct pagetable_do_unmap_data data = { .do_unref = do_unref };
    pml5_traverse(max.max, &vaddr, &n_pages, pagetable_do_unmap_callbacks, &data);
    if (pml5_destroy_range(max.max, &vaddr1, &n_pages1))
    {
        // entire pml5 was destroyed?
        MOS_UNREACHABLE();
    }
}

void mm_do_mask_flags(pgd_t max, ptr_t vaddr, size_t n_pages, vm_flags mask)
{
    struct pagetable_do_mask_data data = { .mask = mask };
    pml5_traverse(max.max, &vaddr, &n_pages, pagetable_do_mask_callbacks, &data);
}

void mm_do_copy(pgd_t src, pgd_t dst, ptr_t vaddr, size_t n_pages)
{
    struct pagetable_do_copy_data data = {
        .dest_pml5 = dst.max,
        .dest_pml5e = pml5_entry(dst.max, vaddr),
        .dest_pml4 = pml5e_get_or_create_pml4(data.dest_pml5e),
    };
    pml5_traverse(src.max, &vaddr, &n_pages, pagetable_do_copy_callbacks, &data);
}

pfn_t mm_do_get_pfn(pgd_t max, ptr_t vaddr)
{
    vaddr = ALIGN_DOWN_TO_PAGE(vaddr);
    pml5e_t *pml5e = pml5_entry(max.max, vaddr);
    if (!pml5e_is_present(pml5e))
        return 0;

    const pml4_t pml4 = pml5e_get_or_create_pml4(pml5e);
    pml4e_t *pml4e = pml4_entry(pml4, vaddr);
    if (!pml4e_is_present(pml4e))
        return 0;

#if MOS_CONFIG(PML4_HUGE_CAPABLE)
    if (platform_pml4e_is_huge(pml4e))
        return platform_pml4e_get_huge_pfn(pml4e) + (vaddr & PML4_HUGE_MASK) / MOS_PAGE_SIZE;
#endif

    const pml3_t pml3 = pml4e_get_or_create_pml3(pml4e);
    pml3e_t *pml3e = pml3_entry(pml3, vaddr);
    if (!pml3e_is_present(pml3e))
        return 0;

#if MOS_CONFIG(PML3_HUGE_CAPABLE)
    if (platform_pml3e_is_huge(pml3e))
        return platform_pml3e_get_huge_pfn(pml3e) + (vaddr & PML3_HUGE_MASK) / MOS_PAGE_SIZE;
#endif

    const pml2_t pml2 = pml3e_get_or_create_pml2(pml3e);
    pml2e_t *pml2e = pml2_entry(pml2, vaddr);
    if (!pml2e_is_present(pml2e))
        return 0;

#if MOS_CONFIG(PML2_HUGE_CAPABLE)
    if (platform_pml2e_is_huge(pml2e))
        return platform_pml2e_get_huge_pfn(pml2e) + (vaddr & PML2_HUGE_MASK) / MOS_PAGE_SIZE;
#endif

    const pml1_t pml1 = pml2e_get_or_create_pml1(pml2e);
    const pml1e_t *pml1e = pml1_entry(pml1, vaddr);
    if (!pml1e_is_present(pml1e))
        return 0;

    return pml1e_get_pfn(pml1e);
}

vm_flags mm_do_get_flags(pgd_t max, ptr_t vaddr)
{
    vm_flags flags = ~0;
    vaddr = ALIGN_DOWN_TO_PAGE(vaddr);
    pml5e_t *pml5e = pml5_entry(max.max, vaddr);
    if (!pml5e_is_present(pml5e))
        return 0;

    const pml4_t pml4 = pml5e_get_or_create_pml4(pml5e);
    pml4e_t *pml4e = pml4_entry(pml4, vaddr);
    if (!pml4e_is_present(pml4e))
        return 0;

#if MOS_CONFIG(PML4_HUGE_CAPABLE)
    if (platform_pml4e_is_huge(pml4e))
        return platform_pml4e_get_flags(pml4e);
    else
        flags &= platform_pml4e_get_flags(pml4e);
#else
    flags &= platform_pml4e_get_flags(pml4e);
#endif

    const pml3_t pml3 = pml4e_get_or_create_pml3(pml4e);
    pml3e_t *pml3e = pml3_entry(pml3, vaddr);
    if (!pml3e_is_present(pml3e))
        return 0;

#if MOS_CONFIG(PML3_HUGE_CAPABLE)
    if (platform_pml3e_is_huge(pml3e))
        return platform_pml3e_get_flags(pml3e);
    else
        flags &= platform_pml3e_get_flags(pml3e);
#else
    flags &= platform_pml3e_get_flags(pml3e);
#endif

    const pml2_t pml2 = pml3e_get_or_create_pml2(pml3e);
    pml2e_t *pml2e = pml2_entry(pml2, vaddr);
    if (!pml2e_is_present(pml2e))
        return 0;

#if MOS_CONFIG(PML2_HUGE_CAPABLE)
    if (platform_pml2e_is_huge(pml2e))
        return platform_pml2e_get_flags(pml2e);
    else
        flags &= platform_pml2e_get_flags(pml2e);
#else
    flags &= platform_pml2e_get_flags(pml2e);
#endif

    const pml1_t pml1 = pml2e_get_or_create_pml1(pml2e);
    const pml1e_t *pml1e = pml1_entry(pml1, vaddr);
    if (!pml1e_is_present(pml1e))
        return 0;

    flags &= platform_pml1e_get_flags(pml1e);
    return flags;
}

void *__create_page_table(void)
{
    mmstat_inc1(MEM_PAGETABLE);
    return (void *) phyframe_va(mm_get_free_page());
}

void __destroy_page_table(void *table)
{
    mmstat_dec1(MEM_PAGETABLE);
    mm_free_page(va_phyframe(table));
}
