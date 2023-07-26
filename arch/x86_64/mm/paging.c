// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/paging.h"

#include "mos/mm/mm.h"
#include "mos/mm/paging/pmlx/pml2.h"
#include "mos/mm/paging/pmlx/pml3.h"
#include "mos/mm/paging/pmlx/pml4.h"
#include "mos/mm/paging/table_ops.h"
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

    mos_debug(x86_startup, "mapping kernel space...");

    x86_platform.k_code.vaddr = (ptr_t) __MOS_KERNEL_CODE_START;
    x86_platform.k_rodata.vaddr = (ptr_t) __MOS_KERNEL_RODATA_START;
    x86_platform.k_rwdata.vaddr = (ptr_t) __MOS_KERNEL_RW_START;

    x86_platform.k_code.npages = ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_CODE_END - (ptr_t) __MOS_KERNEL_CODE_START) / MOS_PAGE_SIZE;
    x86_platform.k_rodata.npages = ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_RODATA_END - (ptr_t) __MOS_KERNEL_RODATA_START) / MOS_PAGE_SIZE;
    x86_platform.k_rwdata.npages = ALIGN_UP_TO_PAGE((ptr_t) __MOS_KERNEL_RW_END - (ptr_t) __MOS_KERNEL_RW_START) / MOS_PAGE_SIZE;

    // no need to reserve the kernel space, bootloader has done this
    x86_platform.k_code = mm_map_pages(            //
        platform_info->kernel_mm,                  //
        x86_platform.k_code.vaddr,                 //
        MOS_KERNEL_PFN(x86_platform.k_code.vaddr), //
        x86_platform.k_code.npages,                //
        VM_READ | VM_EXEC | VM_GLOBAL              //
    );

    x86_platform.k_rodata = mm_map_pages(            //
        platform_info->kernel_mm,                    //
        x86_platform.k_rodata.vaddr,                 //
        MOS_KERNEL_PFN(x86_platform.k_rodata.vaddr), //
        x86_platform.k_rodata.npages,                //
        VM_READ | VM_GLOBAL                          //
    );

    x86_platform.k_rwdata = mm_map_pages(            //
        platform_info->kernel_mm,                    //
        x86_platform.k_rwdata.vaddr,                 //
        MOS_KERNEL_PFN(x86_platform.k_rwdata.vaddr), //
        x86_platform.k_rwdata.npages,                //
        VM_READ | VM_WRITE | VM_GLOBAL               //
    );

    // map all memory to MOS_DIRECT_MAP_VADDR using 1 GiB, 2 MiB, or 4 KiB pages
    // check if 1 GiB pages are supported
    x86_cpuid_info_t cpuid = { 0 };
    x86_call_cpuid(0x80000001, &cpuid);
    const bool gbpages = cpuid.edx & (1 << 26);

    pr_info2("mapping all memory to " PTR_FMT " using %s pages", x86_platform.direct_map_base, gbpages ? "1 GB" : "2 MB");

    pfn_t pfn = 0;

    const size_t STEP = (gbpages ? 1 GB : 2 MB) / MOS_PAGE_SIZE;
    const size_t total_npages = ALIGN_UP(platform_info->max_pfn, STEP);

    while (pfn < total_npages)
    {
        const ptr_t vaddr = pfn_va(pfn);
        pml4e_t *pml4e = pml4_entry(pml4, vaddr);
        platform_pml4e_set_flags(pml4e, VM_READ | VM_WRITE | VM_GLOBAL);

        if (gbpages)
        {
            // GB pages are at pml3e level
            const pml3_t pml3 = pml4e_get_pml3(pml4e);
            pml3e_t *pml3e = pml3_entry(pml3, vaddr);
            platform_pml3e_set_huge(pml3e, pfn);
            platform_pml3e_set_flags(pml3e, VM_READ | VM_WRITE | VM_GLOBAL);
        }
        else
        {
            // 2 MiB pages are at pml2e level
            pml3e_t *pml3e = pml3_entry(pml4e_get_pml3(pml4e), vaddr);
            platform_pml3e_set_flags(pml3e, VM_READ | VM_WRITE | VM_GLOBAL);

            const pml2_t pml2 = pml3e_get_pml2(pml3e);
            pml2e_t *pml2e = pml2_entry(pml2, vaddr);
            platform_pml2e_set_huge(pml2e, pfn);
            platform_pml2e_set_flags(pml2e, VM_READ | VM_WRITE | VM_GLOBAL);
        }

        pfn += STEP;
    }
}

void x86_enable_paging_impl(ptr_t phy)
{
    x86_cpu_set_cr3(phy);
    __asm__ volatile("mov %%cr4, %%rax; orq $0x80, %%rax; mov %%rax, %%cr4" ::: "rax");
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
    entry->writable = flags & VM_RW;
    entry->usermode = flags & VM_USER;
    entry->write_through = flags & VM_WRITE_THROUGH;
    entry->cache_disabled = flags & VM_CACHE_DISABLED;
    entry->global = flags & VM_GLOBAL;
    entry->no_execute = !(flags & VM_EXEC);
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
    return entry->pfn;
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
    if (entry->page_size)
        entry->no_execute = !(flags & VM_EXEC); // if huge, set NX bit accordingly
    else if (flags & VM_EXEC)
        entry->no_execute = false; // if not huge, set NX bit only if exec flag is set
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
    return entry->pfn;
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

#if MOS_BITS == 32
struct x86_pg_infra_t
{
    x86_pde32_t pgdir[1024];
    x86_pte32_t pgtable[1024 * 1024];
} *x86_kpg_infra;

void x86_mm_walk_page_table(mm_context_t *mmctx, ptr_t vaddr_start, size_t n_pages, pgt_iteration_callback_t callback, void *arg)
{
    return;
    MOS_UNREACHABLE();
    ptr_t vaddr = vaddr_start;
    size_t n_pages_left = n_pages;

    const struct x86_pg_infra_t *pg = NULL; // handle->pagetable.table;
    const pgt_iteration_info_t info = { .address_space = mmctx, .vaddr_start = vaddr_start, .npages = n_pages };

    pfn_t previous_pfn = 0;
    vm_flags previous_flags = 0;
    bool previous_present = false;
    vmblock_t previous_block = { .vaddr = vaddr_start, .npages = 0, .flags = 0, .address_space = mmctx };

    do
    {
        const id_t pgd_i = vaddr >> 22;
        const id_t pgt_i = (vaddr >> 12) & 0x3FF;

        const x86_pde32_t *pgd = (vaddr >= MOS_KERNEL_START_VADDR) ? &x86_kpg_infra->pgdir[pgd_i] : &pg->pgdir[pgd_i];
        const x86_pte32_t *pgt = (vaddr >= MOS_KERNEL_START_VADDR) ? &x86_kpg_infra->pgtable[pgd_i * 1024 + pgt_i] : &pg->pgtable[pgd_i * 1024 + pgt_i];

        const bool present = pgd->present && pgt->present;
        if (present == previous_present && !present)
        {
            if (!pgd->present && pgt_i == 0)
            {
                vaddr += MOS_PAGE_SIZE * 1024;
                n_pages_left = (n_pages_left > 1024) ? n_pages_left - 1024 : 0;
            }
            else
            {
                vaddr += MOS_PAGE_SIZE;
                n_pages_left--;
            }
            continue;
        }

        const pfn_t pfn = pgt->pfn;
        const vm_flags flags = VM_READ |                                                              //
                               (pgd->writable && pgt->writable ? VM_WRITE : 0) |                      //
                               (pgd->usermode && pgt->usermode ? VM_USER : 0) |                       //
                               (pgd->cache_disabled && pgt->cache_disabled ? VM_CACHE_DISABLED : 0) | //
                               (pgt->global ? VM_GLOBAL : 0);

        // if anything changed, call the callback
        if (present != previous_present || pfn != previous_pfn + 1 || flags != previous_flags)
        {
            if (previous_block.npages > 0 && previous_present)
                callback(&info, &previous_block, previous_pfn - (previous_block.npages - 1), arg);

            previous_block.vaddr = vaddr;
            previous_block.npages = 1;
            previous_block.flags = flags;
            previous_pfn = pfn;
            previous_present = present;
            previous_flags = flags;
        }
        else
        {
            previous_block.npages++;
            previous_pfn = pfn;
        }

        vaddr += MOS_PAGE_SIZE;
        n_pages_left--;
    } while (n_pages_left > 0);

    if (previous_block.npages > 0 && previous_present)
        callback(&info, &previous_block, previous_pfn - (previous_block.npages - 1), arg);
}
#else
void x86_mm_walk_page_table(mm_context_t *mmctx, ptr_t vaddr_start, size_t n_pages, pgt_iteration_callback_t callback, void *arg)
{
    MOS_UNUSED(mmctx);
    MOS_UNUSED(vaddr_start);
    MOS_UNUSED(n_pages);
    MOS_UNUSED(callback);
    MOS_UNUSED(arg);

    pr_emerg("STUB STUB STUB STUB STUB");
}
#endif
