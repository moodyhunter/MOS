// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/mm.hpp"
#include "mos/mm/paging/pml_types.hpp"
#include "mos/mm/paging/pmlx/pml2.hpp"
#include "mos/mm/paging/pmlx/pml3.hpp"
#include "mos/mm/paging/pmlx/pml4.hpp"
#include "mos/platform/platform.hpp"
#include "mos/syslog/printk.hpp"
#include "mos/x86/cpu/cpuid.hpp"
#include "mos/x86/mm/paging_impl.hpp"
#include "mos/x86/x86_platform.hpp"

#include <algorithm>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/types.hpp>
#include <mos_stdlib.hpp>

static void x86_setup_direct_map(pml4_t pml4)
{
    // map all memory to MOS_DIRECT_MAP_VADDR using 1 GiB, or 2 MiB pages
    const bool gbpages = cpu_has_feature(CPU_FEATURE_PDPE1GB);

    pr_dinfo2(x86_startup, "mapping all memory to " PTR_FMT " using %s pages", x86_platform.direct_map_base, gbpages ? "1 GB" : "2 MB");

    const size_t STEP = (gbpages ? 1 GB : 2 MB) / MOS_PAGE_SIZE;
    const size_t total_npages = std::max(ALIGN_UP(platform_info->max_pfn, STEP), 4 GB / MOS_PAGE_SIZE);

    pfn_t pfn = 0;
    while (pfn < total_npages)
    {
        const ptr_t vaddr = pfn_va(pfn);
        pml4e_t *pml4e = pml4_entry(pml4, vaddr);
        platform_pml4e_set_flags(pml4e, VM_READ | VM_WRITE | VM_GLOBAL);

        if (gbpages)
        {
            // GB pages are at pml3e level
            const pml3_t pml3 = pml4e_get_or_create_pml3(pml4e);
            pml3e_t *pml3e = pml3_entry(pml3, vaddr);
            platform_pml3e_set_huge(pml3e, pfn);
            platform_pml3e_set_flags(pml3e, VM_READ | VM_WRITE | VM_GLOBAL);
        }
        else
        {
            // 2 MiB pages are at pml2e level
            pml3e_t *pml3e = pml3_entry(pml4e_get_or_create_pml3(pml4e), vaddr);
            platform_pml3e_set_flags(pml3e, VM_READ | VM_WRITE | VM_GLOBAL);

            const pml2_t pml2 = pml3e_get_or_create_pml2(pml3e);
            pml2e_t *pml2e = pml2_entry(pml2, vaddr);
            platform_pml2e_set_huge(pml2e, pfn);
            platform_pml2e_set_flags(pml2e, VM_READ | VM_WRITE | VM_GLOBAL);
        }

        pfn += STEP;
    }
}

void x86_paging_setup()
{
    x86_setup_direct_map(platform_info->kernel_mm->pgd.max.next);
}

pfn_t platform_pml1e_get_pfn(const pml1e_t *pml1e)
{
    const x86_pte64_t *entry = cast<x86_pte64_t>(pml1e);
    return entry->pfn;
}

void platform_pml1e_set_pfn(pml1e_t *pml1e, pfn_t pfn)
{
    x86_pte64_t *entry = cast<x86_pte64_t>(pml1e);
    entry->present = true;
    entry->pfn = pfn;
}

bool platform_pml1e_get_present(const pml1e_t *pml1e)
{
    const x86_pte64_t *entry = cast<x86_pte64_t>(pml1e);
    return entry->present;
}

void platform_pml1e_set_flags(pml1e_t *pml1e, VMFlags flags)
{
    x86_pte64_t *entry = cast<x86_pte64_t>(pml1e);
    entry->writable = flags & VM_WRITE;
    entry->usermode = flags & VM_USER;
    entry->write_through = flags & VM_WRITE_THROUGH;
    entry->cache_disabled = flags & VM_CACHE_DISABLED;
    entry->global = flags & VM_GLOBAL;
    entry->no_execute = !(flags & VM_EXEC);
}

VMFlags platform_pml1e_get_flags(const pml1e_t *pml1e)
{
    const x86_pte64_t *entry = cast<x86_pte64_t>(pml1e);
    VMFlags flags = VM_READ;
    flags |= entry->writable ? VM_WRITE : VM_NONE;
    flags |= entry->usermode ? VM_USER : VM_NONE;
    flags |= entry->write_through ? VM_WRITE_THROUGH : VM_NONE;
    flags |= entry->cache_disabled ? VM_CACHE_DISABLED : VM_NONE;
    flags |= entry->global ? VM_GLOBAL : VM_NONE;
    flags |= entry->no_execute ? VM_NONE : VM_EXEC;
    return flags;
}

// PML2

pml1_t platform_pml2e_get_pml1(const pml2e_t *pml2e)
{
    const x86_pde64_t *entry = cast<x86_pde64_t>(pml2e);
    return { .table = (pml1e_t *) pfn_va(entry->page_table_paddr) };
}

void platform_pml2e_set_pml1(pml2e_t *pml2e, pml1_t pml1, pfn_t pml1_pfn)
{
    MOS_UNUSED(pml1);
    x86_pde64_t *entry = cast<x86_pde64_t>(pml2e);
    entry->present = true;
    entry->page_table_paddr = pml1_pfn;
}

bool platform_pml2e_get_present(const pml2e_t *pml2e)
{
    const x86_pde64_t *entry = cast<x86_pde64_t>(pml2e);
    return entry->present;
}

void platform_pml2e_set_flags(pml2e_t *pml2e, VMFlags flags)
{
    x86_pde64_t *entry = cast<x86_pde64_t>(pml2e);
    entry->writable |= flags & VM_WRITE;
    entry->usermode |= flags & VM_USER;
    entry->write_through |= flags & VM_WRITE_THROUGH;
    entry->cache_disabled |= flags & VM_CACHE_DISABLED;
    if (flags & VM_EXEC)
        entry->no_execute = false;

    if (entry->page_size)
    {
        entry->no_execute = !(flags & VM_EXEC);
        x86_pde64_huge_t *huge_entry = cast<x86_pde64_huge_t>(pml2e);
        huge_entry->global = flags & VM_GLOBAL;
    }
}

VMFlags platform_pml2e_get_flags(const pml2e_t *pml2e)
{
    const x86_pde64_t *entry = cast<x86_pde64_t>(pml2e);
    VMFlags flags = VM_READ;
    flags |= entry->writable ? VM_WRITE : VM_NONE;
    flags |= entry->usermode ? VM_USER : VM_NONE;
    flags |= entry->write_through ? VM_WRITE_THROUGH : VM_NONE;
    flags |= entry->cache_disabled ? VM_CACHE_DISABLED : VM_NONE;
    flags |= entry->no_execute ? VM_NONE : VM_EXEC;
    if (entry->page_size)
    {
        const x86_pde64_huge_t *huge_entry = cast<x86_pde64_huge_t>(pml2e);
        flags |= huge_entry->global ? VM_GLOBAL : VM_NONE;
    }

    return flags;
}

bool platform_pml2e_is_huge(const pml2e_t *pml2e)
{
    const x86_pde64_t *entry = cast<x86_pde64_t>(pml2e);
    return entry->page_size;
}

void platform_pml2e_set_huge(pml2e_t *pml2e, pfn_t pfn)
{
    pml2e->content = 0;
    x86_pde64_huge_t *entry = cast<x86_pde64_huge_t>(pml2e);
    entry->present = true;
    entry->page_size = true;
    entry->pfn = pfn >> 1; // 1 bit PAT
}

pfn_t platform_pml2e_get_huge_pfn(const pml2e_t *pml2)
{
    const x86_pde64_huge_t *entry = cast<x86_pde64_huge_t>(pml2);
    return entry->pfn << 1; // 1 bit PAT
}

// PML3

pml2_t platform_pml3e_get_pml2(const pml3e_t *pml3e)
{
    const x86_pmde64_t *entry = cast<x86_pmde64_t>(pml3e);
    return { .table = (pml2e_t *) pfn_va(entry->page_table_paddr) };
}

void platform_pml3e_set_pml2(pml3e_t *pml3e, pml2_t pml2, pfn_t pml2_pfn)
{
    MOS_UNUSED(pml2);
    x86_pmde64_t *entry = cast<x86_pmde64_t>(pml3e);
    entry->present = true;
    entry->page_table_paddr = pml2_pfn;
}

bool platform_pml3e_get_present(const pml3e_t *pml3e)
{
    const x86_pmde64_t *entry = cast<x86_pmde64_t>(pml3e);
    return entry->present;
}

void platform_pml3e_set_flags(pml3e_t *pml3e, VMFlags flags)
{
    x86_pmde64_t *entry = cast<x86_pmde64_t>(pml3e);
    entry->writable |= flags & VM_WRITE;
    entry->usermode |= flags & VM_USER;
    entry->write_through |= flags & VM_WRITE_THROUGH;
    entry->cache_disabled |= flags & VM_CACHE_DISABLED;
    if (flags & VM_EXEC)
        entry->no_execute = false; // if not huge, set NX bit only if exec flag is set

    if (entry->page_size)
    {
        entry->no_execute = !(flags & VM_EXEC); // if huge, set NX bit accordingly
        x86_pmde64_huge_t *huge_entry = cast<x86_pmde64_huge_t>(pml3e);
        huge_entry->global = flags & VM_GLOBAL;
    }
}

VMFlags platform_pml3e_get_flags(const pml3e_t *pml3e)
{
    const x86_pmde64_t *entry = cast<x86_pmde64_t>(pml3e);
    VMFlags flags = VM_READ;
    flags |= entry->writable ? VM_WRITE : VM_NONE;
    flags |= entry->usermode ? VM_USER : VM_NONE;
    flags |= entry->write_through ? VM_WRITE_THROUGH : VM_NONE;
    flags |= entry->cache_disabled ? VM_CACHE_DISABLED : VM_NONE;
    flags |= entry->no_execute ? VM_NONE : VM_EXEC;
    if (entry->page_size)
    {
        const x86_pmde64_huge_t *huge_entry = cast<x86_pmde64_huge_t>(pml3e);
        flags |= huge_entry->global ? VM_GLOBAL : VM_NONE;
    }
    return flags;
}

bool platform_pml3e_is_huge(const pml3e_t *pml3e)
{
    const x86_pmde64_t *entry = cast<x86_pmde64_t>(pml3e);
    return entry->page_size;
}

void platform_pml3e_set_huge(pml3e_t *pml3e, pfn_t pfn)
{
    pml3e->content = 0;
    x86_pmde64_huge_t *entry = cast<x86_pmde64_huge_t>(pml3e);
    entry->present = true;
    entry->page_size = true;
    entry->pfn = pfn >> 1; // 1 bit PAT
}

pfn_t platform_pml3e_get_huge_pfn(const pml3e_t *pml3)
{
    const x86_pmde64_huge_t *entry = cast<x86_pmde64_huge_t>(pml3);
    return entry->pfn << 1; // 1 bit PAT
}

// PML4

pml3_t platform_pml4e_get_pml3(const pml4e_t *pml4e)
{
    const x86_pude64_t *entry = cast<x86_pude64_t>(pml4e);
    return { .table = (pml3e_t *) pfn_va(entry->page_table_paddr) };
}

void platform_pml4e_set_pml3(pml4e_t *pml4e, pml3_t pml3, pfn_t pml3_pfn)
{
    MOS_UNUSED(pml3);
    x86_pude64_t *entry = cast<x86_pude64_t>(pml4e);
    entry->present = true;
    entry->page_table_paddr = pml3_pfn;
}

bool platform_pml4e_get_present(const pml4e_t *pml4e)
{
    const x86_pude64_t *entry = cast<x86_pude64_t>(pml4e);
    return entry->present;
}

void platform_pml4e_set_flags(pml4e_t *pml4e, VMFlags flags)
{
    x86_pude64_t *entry = cast<x86_pude64_t>(pml4e);
    entry->writable |= flags & VM_WRITE;
    entry->usermode |= flags & VM_USER;
    entry->write_through |= flags & VM_WRITE_THROUGH;
    entry->cache_disabled |= flags & VM_CACHE_DISABLED;
    if (flags & VM_EXEC)
        entry->no_execute = false;
}

VMFlags platform_pml4e_get_flags(const pml4e_t *pml4e)
{
    const x86_pude64_t *entry = cast<x86_pude64_t>(pml4e);
    VMFlags flags = VM_READ;
    if (entry->writable)
        flags |= VM_WRITE;
    flags |= entry->writable ? VM_WRITE : VM_NONE;
    flags |= entry->usermode ? VM_USER : VM_NONE;
    flags |= entry->write_through ? VM_WRITE_THROUGH : VM_NONE;
    flags |= entry->cache_disabled ? VM_CACHE_DISABLED : VM_NONE;
    flags |= entry->no_execute ? VM_NONE : VM_EXEC;
    return flags;
}
