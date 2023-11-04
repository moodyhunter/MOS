// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/paging.h"

#include "mos/mm/mm.h"
#include "mos/mm/paging/pmlx/pml2.h"
#include "mos/mm/paging/pmlx/pml3.h"
#include "mos/mm/paging/pmlx/pml4.h"
#include "mos/mm/physical/pmm.h"
#include "mos/platform/platform.h"
#include "mos/platform/platform_defs.h"
#include "mos/x86/cpu/cpu.h"
#include "mos/x86/cpu/cpuid.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/x86_platform.h"

#include <mos/lib/structures/list.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/types.h>
#include <mos_stdlib.h>

static mm_context_t x86_kernel_mmctx = {
    .mm_lock = SPINLOCK_INIT,
    .mmaps = LIST_HEAD_INIT(x86_kernel_mmctx.mmaps),
};

void x86_paging_setup()
{
    platform_info->kernel_mm = &x86_kernel_mmctx;
    current_cpu->mm_context = &x86_kernel_mmctx;

    const pml4_t pml4 = pml_create_table(pml4);
    x86_kernel_mmctx.pgd = pgd_create(pml4);

    pr_dinfo2(x86_startup, "mapping kernel space...");

    // no need to reserve the kernel space, bootloader has done this
    mm_map_kernel_pages(                                                                                   //
        platform_info->kernel_mm,                                                                          //
        (ptr_t) __MOS_KERNEL_CODE_START,                                                                   //
        MOS_KERNEL_PFN((ptr_t) __MOS_KERNEL_CODE_START),                                                   //
        ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_CODE_END - (ptr_t) __MOS_KERNEL_CODE_START) / MOS_PAGE_SIZE, //
        VM_READ | VM_EXEC | VM_GLOBAL                                                                      //
    );

    mm_map_kernel_pages(                                                                                       //
        platform_info->kernel_mm,                                                                              //
        (ptr_t) __MOS_KERNEL_RODATA_START,                                                                     //
        MOS_KERNEL_PFN((ptr_t) __MOS_KERNEL_RODATA_START),                                                     //
        ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_RODATA_END - (ptr_t) __MOS_KERNEL_RODATA_START) / MOS_PAGE_SIZE, //
        VM_READ | VM_GLOBAL                                                                                    //
    );

    mm_map_kernel_pages(                                                                               //
        platform_info->kernel_mm,                                                                      //
        (ptr_t) __MOS_KERNEL_RW_START,                                                                 //
        MOS_KERNEL_PFN((ptr_t) __MOS_KERNEL_RW_START),                                                 //
        ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_RW_END - (ptr_t) __MOS_KERNEL_RW_START) / MOS_PAGE_SIZE, //
        VM_READ | VM_WRITE | VM_GLOBAL                                                                 //
    );

    // map all memory to MOS_DIRECT_MAP_VADDR using 1 GiB, or 2 MiB pages
    const bool gbpages = cpu_has_feature(CPU_FEATURE_PDPE1GB);

    pr_dinfo2(x86_startup, "mapping all memory to " PTR_FMT " using %s pages", x86_platform.direct_map_base, gbpages ? "1 GB" : "2 MB");

    const size_t STEP = (gbpages ? 1 GB : 2 MB) / MOS_PAGE_SIZE;
    const size_t total_npages = MAX(ALIGN_UP(platform_info->max_pfn, STEP), 4 GB / MOS_PAGE_SIZE);

    pfn_t pfn = 0;
    while (pfn < total_npages)
    {
        const ptr_t vaddr = pfn_va(pfn);
        pml4e_t *pml4e = pml4_entry(pml4, vaddr);
        platform_pml4e_set_flags(pml4e, VM_READ | VM_WRITE | VM_GLOBAL | VM_CACHE_DISABLED);

        if (gbpages)
        {
            // GB pages are at pml3e level
            const pml3_t pml3 = pml4e_get_or_create_pml3(pml4e);
            pml3e_t *pml3e = pml3_entry(pml3, vaddr);
            platform_pml3e_set_huge(pml3e, pfn);
            platform_pml3e_set_flags(pml3e, VM_READ | VM_WRITE | VM_GLOBAL | VM_CACHE_DISABLED);
        }
        else
        {
            // 2 MiB pages are at pml2e level
            pml3e_t *pml3e = pml3_entry(pml4e_get_or_create_pml3(pml4e), vaddr);
            platform_pml3e_set_flags(pml3e, VM_READ | VM_WRITE | VM_GLOBAL | VM_CACHE_DISABLED);

            const pml2_t pml2 = pml3e_get_or_create_pml2(pml3e);
            pml2e_t *pml2e = pml2_entry(pml2, vaddr);
            platform_pml2e_set_huge(pml2e, pfn);
            platform_pml2e_set_flags(pml2e, VM_READ | VM_WRITE | VM_GLOBAL | VM_CACHE_DISABLED);
        }

        pfn += STEP;
    }
}

pfn_t platform_pml1e_get_pfn(const pml1e_t *pml1e)
{
    const x86_pte64_t *entry = cast_to(pml1e, pml1e_t *, x86_pte64_t *);
    return entry->pfn;
}

void platform_pml1e_set_pfn(pml1e_t *pml1e, pfn_t pfn)
{
    x86_pte64_t *entry = cast_to(pml1e, pml1e_t *, x86_pte64_t *);
    entry->pfn = pfn;
}

bool platform_pml1e_get_present(const pml1e_t *pml1e)
{
    const x86_pte64_t *entry = cast_to(pml1e, pml1e_t *, x86_pte64_t *);
    return entry->present;
}

void platform_pml1e_set_present(pml1e_t *pml1e, bool present)
{
    pml1e->content = 0; // clear all flags
    x86_pte64_t *entry = cast_to(pml1e, pml1e_t *, x86_pte64_t *);
    entry->present = present;
}

void platform_pml1e_set_flags(pml1e_t *pml1e, vm_flags flags)
{
    x86_pte64_t *entry = cast_to(pml1e, pml1e_t *, x86_pte64_t *);
    entry->writable = flags & VM_WRITE;
    entry->usermode = flags & VM_USER;
    entry->write_through = flags & VM_WRITE_THROUGH;
    entry->cache_disabled = flags & VM_CACHE_DISABLED;
    entry->global = flags & VM_GLOBAL;
    entry->no_execute = !(flags & VM_EXEC);
}

vm_flags platform_pml1e_get_flags(const pml1e_t *pml1e)
{
    const x86_pte64_t *entry = cast_to(pml1e, pml1e_t *, x86_pte64_t *);
    vm_flags flags = VM_READ;
    flags |= entry->writable ? VM_WRITE : 0;
    flags |= entry->usermode ? VM_USER : 0;
    flags |= entry->write_through ? VM_WRITE_THROUGH : 0;
    flags |= entry->cache_disabled ? VM_CACHE_DISABLED : 0;
    flags |= entry->global ? VM_GLOBAL : 0;
    flags |= entry->no_execute ? 0 : VM_EXEC;
    return flags;
}

// PML2

pml1_t platform_pml2e_get_pml1(const pml2e_t *pml2e)
{
    const x86_pde64_t *entry = cast_to(pml2e, pml2e_t *, x86_pde64_t *);
    return (pml1_t){ .table = (pml1e_t *) pfn_va(entry->page_table_paddr) };
}

void platform_pml2e_set_pml1(pml2e_t *pml2e, pml1_t pml1, pfn_t pml1_pfn)
{
    MOS_UNUSED(pml1);
    x86_pde64_t *entry = cast_to(pml2e, pml2e_t *, x86_pde64_t *);
    entry->page_table_paddr = pml1_pfn;
}

bool platform_pml2e_get_present(const pml2e_t *pml2e)
{
    const x86_pde64_t *entry = cast_to(pml2e, pml2e_t *, x86_pde64_t *);
    return entry->present;
}

void platform_pml2e_set_present(pml2e_t *pml2e, bool present)
{
    pml2e->content = 0; // clear all flags
    x86_pde64_t *entry = cast_to(pml2e, pml2e_t *, x86_pde64_t *);
    entry->present = present;
}

void platform_pml2e_set_flags(pml2e_t *pml2e, vm_flags flags)
{
    x86_pde64_t *entry = cast_to(pml2e, pml2e_t *, x86_pde64_t *);
    entry->writable |= flags & VM_WRITE;
    entry->usermode |= flags & VM_USER;
    entry->write_through |= flags & VM_WRITE_THROUGH;
    entry->cache_disabled |= flags & VM_CACHE_DISABLED;
    if (flags & VM_EXEC)
        entry->no_execute = false;

    if (entry->page_size)
    {
        entry->no_execute = !(flags & VM_EXEC);
        x86_pde64_huge_t *huge_entry = cast_to(entry, x86_pde64_t *, x86_pde64_huge_t *);
        huge_entry->global = flags & VM_GLOBAL;
    }
}

vm_flags platform_pml2e_get_flags(const pml2e_t *pml2e)
{
    const x86_pde64_t *entry = cast_to(pml2e, pml2e_t *, x86_pde64_t *);
    vm_flags flags = VM_READ;
    flags |= entry->writable ? VM_WRITE : 0;
    flags |= entry->usermode ? VM_USER : 0;
    flags |= entry->write_through ? VM_WRITE_THROUGH : 0;
    flags |= entry->cache_disabled ? VM_CACHE_DISABLED : 0;
    flags |= entry->no_execute ? 0 : VM_EXEC;
    if (entry->page_size)
    {
        const x86_pde64_huge_t *huge_entry = cast_to(pml2e, pml2e_t *, x86_pde64_huge_t *);
        flags |= huge_entry->global ? VM_GLOBAL : 0;
    }

    return flags;
}

bool platform_pml2e_is_huge(const pml2e_t *pml2e)
{
    const x86_pde64_t *entry = cast_to(pml2e, pml2e_t *, x86_pde64_t *);
    return entry->page_size;
}

void platform_pml2e_set_huge(pml2e_t *pml2e, pfn_t pfn)
{
    pml2e->content = 0;
    x86_pde64_huge_t *entry = cast_to(pml2e, pml2e_t *, x86_pde64_huge_t *);
    entry->present = true;
    entry->page_size = true;
    entry->pfn = pfn >> 1; // 1 bit PAT
}

pfn_t platform_pml2e_get_huge_pfn(const pml2e_t *pml2)
{
    const x86_pde64_huge_t *entry = cast_to(pml2, pml2e_t *, x86_pde64_huge_t *);
    return entry->pfn << 1; // 1 bit PAT
}

// PML3

pml2_t platform_pml3e_get_pml2(const pml3e_t *pml3e)
{
    const x86_pmde64_t *entry = cast_to(pml3e, pml3e_t *, x86_pmde64_t *);
    return (pml2_t){ .table = (pml2e_t *) pfn_va(entry->page_table_paddr) };
}

void platform_pml3e_set_pml2(pml3e_t *pml3e, pml2_t pml2, pfn_t pml2_pfn)
{
    MOS_UNUSED(pml2);
    x86_pmde64_t *entry = cast_to(pml3e, pml3e_t *, x86_pmde64_t *);
    entry->page_table_paddr = pml2_pfn;
}

bool platform_pml3e_get_present(const pml3e_t *pml3e)
{
    const x86_pmde64_t *entry = cast_to(pml3e, pml3e_t *, x86_pmde64_t *);
    return entry->present;
}

void platform_pml3e_set_present(pml3e_t *pml3e, bool present)
{
    pml3e->content = 0; // clear all flags
    x86_pmde64_t *entry = cast_to(pml3e, pml3e_t *, x86_pmde64_t *);
    entry->present = present;
}

void platform_pml3e_set_flags(pml3e_t *pml3e, vm_flags flags)
{
    x86_pmde64_t *entry = cast_to(pml3e, pml3e_t *, x86_pmde64_t *);
    entry->writable |= flags & VM_WRITE;
    entry->usermode |= flags & VM_USER;
    entry->write_through |= flags & VM_WRITE_THROUGH;
    entry->cache_disabled |= flags & VM_CACHE_DISABLED;
    if (flags & VM_EXEC)
        entry->no_execute = false; // if not huge, set NX bit only if exec flag is set

    if (entry->page_size)
    {
        entry->no_execute = !(flags & VM_EXEC); // if huge, set NX bit accordingly
        x86_pmde64_huge_t *huge_entry = cast_to(entry, x86_pmde64_t *, x86_pmde64_huge_t *);
        huge_entry->global = flags & VM_GLOBAL;
    }
}

vm_flags platform_pml3e_get_flags(const pml3e_t *pml3e)
{
    const x86_pmde64_t *entry = cast_to(pml3e, pml3e_t *, x86_pmde64_t *);
    vm_flags flags = VM_READ;
    flags |= entry->writable ? VM_WRITE : 0;
    flags |= entry->usermode ? VM_USER : 0;
    flags |= entry->write_through ? VM_WRITE_THROUGH : 0;
    flags |= entry->cache_disabled ? VM_CACHE_DISABLED : 0;
    flags |= entry->no_execute ? 0 : VM_EXEC;
    if (entry->page_size)
    {
        const x86_pmde64_huge_t *huge_entry = cast_to(pml3e, pml3e_t *, x86_pmde64_huge_t *);
        flags |= huge_entry->global ? VM_GLOBAL : 0;
    }
    return flags;
}

bool platform_pml3e_is_huge(const pml3e_t *pml3e)
{
    const x86_pmde64_t *entry = cast_to(pml3e, pml3e_t *, x86_pmde64_t *);
    return entry->page_size;
}

void platform_pml3e_set_huge(pml3e_t *pml3e, pfn_t pfn)
{
    pml3e->content = 0;
    x86_pmde64_huge_t *entry = cast_to(pml3e, pml3e_t *, x86_pmde64_huge_t *);
    entry->present = true;
    entry->page_size = true;
    entry->pfn = pfn >> 1; // 1 bit PAT
}

pfn_t platform_pml3e_get_huge_pfn(const pml3e_t *pml3)
{
    const x86_pmde64_huge_t *entry = cast_to(pml3, pml3e_t *, x86_pmde64_huge_t *);
    return entry->pfn << 1; // 1 bit PAT
}

// PML4

pml3_t platform_pml4e_get_pml3(const pml4e_t *pml4e)
{
    const x86_pude64_t *entry = cast_to(pml4e, pml4e_t *, x86_pude64_t *);
    return (pml3_t){ .table = (pml3e_t *) pfn_va(entry->page_table_paddr) };
}

void platform_pml4e_set_pml3(pml4e_t *pml4e, pml3_t pml3, pfn_t pml3_pfn)
{
    MOS_UNUSED(pml3);
    x86_pude64_t *entry = cast_to(pml4e, pml4e_t *, x86_pude64_t *);
    entry->page_table_paddr = pml3_pfn;
}

bool platform_pml4e_get_present(const pml4e_t *pml4e)
{
    const x86_pude64_t *entry = cast_to(pml4e, pml4e_t *, x86_pude64_t *);
    return entry->present;
}

void platform_pml4e_set_present(pml4e_t *pml4e, bool present)
{
    pml4e->content = 0;
    x86_pude64_t *entry = cast_to(pml4e, pml4e_t *, x86_pude64_t *);
    entry->present = present;
}

void platform_pml4e_set_flags(pml4e_t *pml4e, vm_flags flags)
{
    x86_pude64_t *entry = cast_to(pml4e, pml4e_t *, x86_pude64_t *);
    entry->writable |= flags & VM_WRITE;
    entry->usermode |= flags & VM_USER;
    entry->write_through |= flags & VM_WRITE_THROUGH;
    entry->cache_disabled |= flags & VM_CACHE_DISABLED;
    if (flags & VM_EXEC)
        entry->no_execute = false;
}

vm_flags platform_pml4e_get_flags(const pml4e_t *pml4e)
{
    const x86_pude64_t *entry = cast_to(pml4e, pml4e_t *, x86_pude64_t *);
    vm_flags flags = VM_READ;
    flags |= entry->writable ? VM_WRITE : 0;
    flags |= entry->usermode ? VM_USER : 0;
    flags |= entry->write_through ? VM_WRITE_THROUGH : 0;
    flags |= entry->cache_disabled ? VM_CACHE_DISABLED : 0;
    flags |= entry->no_execute ? 0 : VM_EXEC;
    return flags;
}
