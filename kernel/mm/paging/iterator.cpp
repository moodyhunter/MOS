// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/iterator.hpp"

#include "mos/assert.hpp"
#include "mos/mm/paging/pmlx/pml1.hpp"
#include "mos/mm/paging/pmlx/pml2.hpp"
#include "mos/mm/paging/pmlx/pml3.hpp"
#include "mos/mm/paging/pmlx/pml4.hpp"
#include "mos/platform/platform.hpp"

#include <mos_string.hpp>

static void pagetable_iterator_start_current_range(pagetable_iter_t *it, pml5_t *pml5, pml4_t *pml4, pml3_t *pml3, pml2_t *pml2, pml1_t *pml1)
{
    it->range = (pagetable_iter_range_t) { 0 };
    it->range.vaddr = it->vaddr;
    *pml5 = it->pgd.max;

    *pml4 = pml5->next;
    const pml4e_t *pml4e = pml4_entry(*pml4, it->vaddr);
    if (!pml4e_is_present(pml4e))
        return;

#if MOS_CONFIG(PML4_HUGE_CAPABLE)
    if (platform_pml4e_is_huge(pml4e))
    {
        it->range.present = true;
        it->range.flags = platform_pml4e_get_flags(pml4e);
        it->range.pfn = platform_pml4e_get_huge_pfn(pml4e);
        it->vaddr += PML4E_NPAGES * MOS_PAGE_SIZE;
        it->range.pfn_end = it->range.pfn + PML4E_NPAGES - 1;
        return;
    }
#endif

    *pml3 = platform_pml4e_get_pml3(pml4e);
    const pml3e_t *pml3e = pml3_entry(*pml3, it->vaddr);
    if (!pml3e_is_present(pml3e))
        return;

#if MOS_CONFIG(PML3_HUGE_CAPABLE)
    if (platform_pml3e_is_huge(pml3e))
    {
        it->range.present = true;
        it->range.flags = platform_pml3e_get_flags(pml3e);
        it->range.pfn = platform_pml3e_get_huge_pfn(pml3e);
        it->vaddr += PML3E_NPAGES * MOS_PAGE_SIZE;
        it->range.pfn_end = it->range.pfn + PML3E_NPAGES - 1;
        return;
    }
#endif

    *pml2 = platform_pml3e_get_pml2(pml3e);
    const pml2e_t *pml2e = pml2_entry(*pml2, it->vaddr);
    if (!pml2e_is_present(pml2e))
        return;

#if MOS_CONFIG(PML2_HUGE_CAPABLE)
    if (platform_pml2e_is_huge(pml2e))
    {
        it->range.present = true;
        it->range.flags = platform_pml2e_get_flags(pml2e);
        it->range.pfn = platform_pml2e_get_huge_pfn(pml2e);
        it->vaddr += PML2E_NPAGES * MOS_PAGE_SIZE;
        it->range.pfn_end = it->range.pfn + PML2E_NPAGES - 1;
        return;
    }
#endif

    *pml1 = platform_pml2e_get_pml1(pml2e);
    const pml1e_t *pml1e = pml1_entry(*pml1, it->vaddr);
    it->range.present = platform_pml1e_get_present(pml1e);
    if (!it->range.present)
        return;

    it->range.flags = platform_pml1e_get_flags(pml1e);
    it->range.pfn = platform_pml1e_get_pfn(pml1e);
    it->vaddr += PML1E_NPAGES * MOS_PAGE_SIZE;
    it->range.pfn_end = it->range.pfn + PML1E_NPAGES - 1;
}

void pagetable_iter_init(pagetable_iter_t *it, pgd_t pgd, ptr_t vaddr, ptr_t end)
{
    memzero(it, sizeof(*it));
    it->pgd = pgd;
    it->vaddr = it->start = vaddr;
    it->end = end;
}

#define yield_range()                                                                                                                                                    \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        it->range.vaddr_end = it->vaddr - 1;                                                                                                                             \
        return &it->range;                                                                                                                                               \
    } while (0)

pagetable_iter_range_t *pagetable_iter_next(pagetable_iter_t *it)
{
#define _IS_IN_RANGE MOS_IN_RANGE(it->vaddr, it->start, it->end)
    if (!_IS_IN_RANGE)
        return NULL;

    pml5_t pml5 = { 0 };
    pml4_t pml4 = { 0 };
    pml3_t pml3 = { 0 };
    pml2_t pml2 = { 0 };
    pml1_t pml1 = { 0 };

    pagetable_iterator_start_current_range(it, &pml5, &pml4, &pml3, &pml2, &pml1);

    if (!pml_null(pml1))
        goto iterate_pml1;
    else if (!pml_null(pml2))
        goto iterate_pml2;
    else if (!pml_null(pml3))
        goto iterate_pml3;
    else if (!pml_null(pml4))
        goto iterate_pml4;
    else
        MOS_UNREACHABLE();

    __builtin_unreachable();

iterate_pml1:
    for (u32 i = pml1_index(it->vaddr); i < PML1_ENTRIES && _IS_IN_RANGE; i++)
    {
        const pml1e_t *pml1e = &pml1.table[i];
        const bool present = platform_pml1e_get_present(pml1e);

        if (!present && present != it->range.present)
        {
            it->range.vaddr_end = it->vaddr - 1;
            return &it->range;
        }

        if (!present)
        {
            it->vaddr += PML1E_NPAGES * MOS_PAGE_SIZE;
            continue;
        }

        const VMFlags new_flags = platform_pml1e_get_flags(pml1e);
        const pfn_t new_pfn = platform_pml1e_get_pfn(pml1e);

        bool changed = false;
        changed |= new_flags != it->range.flags;     // flags changed
        changed |= new_pfn != it->range.pfn_end + 1; // pfn changed

        if (changed)
            yield_range();

        it->vaddr += PML1E_NPAGES * MOS_PAGE_SIZE; // next time, we start from the next page
        it->range.pfn_end = new_pfn + PML1E_NPAGES - 1;
    }

iterate_pml2:
    for (u32 i = pml2_index(it->vaddr); i < PML2_ENTRIES && _IS_IN_RANGE; i++)
    {
        const pml2e_t *pml2e = &pml2.table[i];
        const bool present = pml2e_is_present(pml2e);

        if (!present && present != it->range.present)
            yield_range();

        if (!present)
        {
            it->vaddr += PML2E_NPAGES * MOS_PAGE_SIZE;
            continue;
        }

#if MOS_CONFIG(PML2_HUGE_CAPABLE)
        if (platform_pml2e_is_huge(pml2e))
        {
            const VMFlags new_flags = platform_pml2e_get_flags(pml2e);
            const pfn_t new_pfn = platform_pml2e_get_huge_pfn(pml2e);

            bool changed = false;
            changed |= new_flags != it->range.flags;     // flags changed
            changed |= new_pfn != it->range.pfn_end + 1; // pfn changed

            if (changed)
                yield_range();

            it->vaddr += PML2E_NPAGES * MOS_PAGE_SIZE;
            it->range.pfn_end = new_pfn + PML2E_NPAGES - 1;
        }
        else
#endif
        {
            pml1 = platform_pml2e_get_pml1(pml2e);
            goto iterate_pml1; // iterate pml1
        }
    }

iterate_pml3:
    for (u32 i = pml3_index(it->vaddr); i < PML3_ENTRIES && _IS_IN_RANGE; i++)
    {
        const pml3e_t *pml3e = pml3_entry(pml3, it->vaddr);
        const bool present = pml3e_is_present(pml3e);

        if (!present && present != it->range.present)
            yield_range();

        if (!present)
        {
            it->vaddr += PML3E_NPAGES * MOS_PAGE_SIZE;
            continue;
        }

#if MOS_CONFIG(PML3_HUGE_CAPABLE)
        if (platform_pml3e_is_huge(pml3e))
        {
            const VMFlags new_flags = platform_pml3e_get_flags(pml3e);
            const pfn_t new_pfn = platform_pml3e_get_huge_pfn(pml3e);

            bool changed = false;
            changed |= new_flags != it->range.flags;     // flags changed
            changed |= new_pfn != it->range.pfn_end + 1; // pfn changed

            if (changed)
                yield_range();

            it->vaddr += PML3E_NPAGES * MOS_PAGE_SIZE;
            it->range.pfn_end = new_pfn + PML3E_NPAGES - 1;
        }
        else
#endif
        {
            pml2 = platform_pml3e_get_pml2(pml3e);
            goto iterate_pml2; // iterate pml2
        }
    }

iterate_pml4:
    for (u32 i = pml4_index(it->vaddr); i < PML4_ENTRIES && _IS_IN_RANGE; i++)
    {
        const pml4e_t *pml4e = pml4_entry(pml4, it->vaddr);
        const bool present = pml4e_is_present(pml4e);

        if (!present && present != it->range.present)
            yield_range();

        if (!present)
        {
            it->vaddr += PML4E_NPAGES * MOS_PAGE_SIZE;
            continue;
        }

        pml3 = platform_pml4e_get_pml3(pml4e);
        goto iterate_pml3; // iterate pml3
    }

    yield_range();
}
