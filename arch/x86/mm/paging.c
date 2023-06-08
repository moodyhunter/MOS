// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/table_ops.h"
#include "mos/mm/physical/pmm.h"

#include <mos/mm/paging/dump.h>
#include <mos/mm/paging/paging.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>
#include <mos/x86/mm/paging.h>
#include <mos/x86/mm/paging_impl.h>
#include <mos/x86/x86_platform.h>
#include <string.h>

#define KERNEL_NPTEs ((MOS_MAX_VADDR - MOS_KERNEL_START_VADDR) / MOS_PAGE_SIZE)
#define KERNEL_NPDEs (1024)

#define KERNEL_PD_START (MOS_KERNEL_START_VADDR / MOS_PAGE_SIZE / 1024)

x86_pte_t init_pte[KERNEL_NPTEs] __aligned(MOS_PAGE_SIZE) = { 0 };  // only for kernel space
x86_pde_t init_pgde[KERNEL_NPDEs] __aligned(MOS_PAGE_SIZE) = { 0 }; // user space and kernel space

MOS_STATIC_ASSERT(sizeof(init_pgde) == MOS_PAGE_SIZE, "init_pgde size should be 4KB");

static spinlock_t x86_kernel_pgd_lock = SPINLOCK_INIT;
static mm_context_t x86_kernel_mm = {
    .lock = &x86_kernel_pgd_lock,
    .um_page_map = NULL,
};

static x86_pg_infra_t *x86_kpg_infra = NULL;

void x86_mm_walk_page_table(mm_context_t *handle, ptr_t vaddr_start, size_t n_pages, pgt_iteration_callback_t callback, void *arg)
{
    return;
    MOS_UNREACHABLE();
    ptr_t vaddr = vaddr_start;
    size_t n_pages_left = n_pages;

    const x86_pg_infra_t *pg = (x86_pg_infra_t *) NULL; // handle->pagetable.table;
    const pgt_iteration_info_t info = { .address_space = handle, .vaddr_start = vaddr_start, .npages = n_pages };

    pfn_t previous_pfn = 0;
    vm_flags previous_flags = 0;
    bool previous_present = false;
    vmblock_t previous_block = { .vaddr = vaddr_start, .npages = 0, .flags = 0, .address_space = handle };

    do
    {
        const id_t pgd_i = vaddr >> 22;
        const id_t pgt_i = (vaddr >> 12) & 0x3FF;

        const x86_pde_t *pgd = (vaddr >= MOS_KERNEL_START_VADDR) ? &x86_kpg_infra->pgdir[pgd_i] : &pg->pgdir[pgd_i];
        const x86_pte_t *pgt = (vaddr >= MOS_KERNEL_START_VADDR) ? &x86_kpg_infra->pgtable[pgd_i * 1024 + pgt_i] : &pg->pgtable[pgd_i * 1024 + pgt_i];

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

void x86_paging_setup()
{
    x86_platform.kernel_mm = &x86_kernel_mm;

    memzero((void *) init_pte, sizeof(init_pte));
    memzero((void *) init_pgde, sizeof(init_pgde));

    pml2_t pml2 = { 0 };
    pml2.table = (pml2e_t *) init_pgde;
    // pml2.table_pfn = MOS_KERNEL_PFN((ptr_t) init_pgde);

    // initialize pml2e
    for (size_t i = KERNEL_PD_START; i < KERNEL_NPDEs; i++)
    {
        pml2e_t *pde = &pml2.table[i];
        x86_pde_t *pgde = cast_to(pde, pml2e_t *, x86_pde_t *);
        pgde->present = true;

        x86_pte_t *pte = &init_pte[(i - KERNEL_PD_START) * 1024];
        pgde->page_table_paddr = MOS_KERNEL_PFN((ptr_t) pte);
        // we can't deduce vaddr from pfn, sadly, s**t x86
        phyframes[pgde->page_table_paddr].platform_info.pagetable_vaddr = (ptr_t) pte;
    }

    x86_kernel_mm.pagetable = mm_pmlmax_create(pml2, MOS_KERNEL_PFN((ptr_t) init_pgde));
    current_cpu->mm_context = &x86_kernel_mm;
}

// PML1

pfn_t platform_pml1e_get_pfn(const pml1e_t *pml1)
{
    const x86_pte_t *entry = cast_to(pml1, pml1e_t *, x86_pte_t *);
    return entry->pfn;
}

void platform_pml1e_set_pfn(pml1e_t *pml1, pfn_t pfn)
{
    x86_pte_t *entry = cast_to(pml1, pml1e_t *, x86_pte_t *);
    entry->pfn = pfn;
}

bool platform_pml1e_get_present(const pml1e_t *pml1)
{
    const x86_pte_t *entry = cast_to(pml1, pml1e_t *, x86_pte_t *);
    return entry->present;
}

void platform_pml1e_set_present(pml1e_t *pml1, bool present)
{
    x86_pte_t *entry = cast_to(pml1, pml1e_t *, x86_pte_t *);
    entry->present = present;
}

void platform_pml1e_set_flags(pml1e_t *pml1, vm_flags flags)
{
    x86_pte_t *entry = cast_to(pml1, pml1e_t *, x86_pte_t *);
    entry->writable = flags & VM_RW;
    entry->usermode = flags & VM_USER;
    entry->write_through = flags & VM_WRITE_THROUGH;
    entry->cache_disabled = flags & VM_CACHE_DISABLED;
    entry->global = flags & VM_GLOBAL;
}

// PML2

pml1_t platform_pml2e_get_pml1(const pml2e_t *pml2)
{
    const x86_pde_t *entry = cast_to(pml2, pml2e_t *, x86_pde_t *);
    return (pml1_t){ .table = (pml1e_t *) phyframes[entry->page_table_paddr].platform_info.pagetable_vaddr };
}

void platform_pml2e_set_pml1(pml2e_t *pml2, pml1_t pml1, pfn_t pml1_pfn)
{
    x86_pde_t *entry = cast_to(pml2, pml2e_t *, x86_pde_t *);
    entry->page_table_paddr = pml1_pfn;
    phyframes[pml1_pfn].platform_info.pagetable_vaddr = (ptr_t) pml1.table; // same as above
}

bool platform_pml2e_get_present(const pml2e_t *pml2)
{
    const x86_pde_t *entry = cast_to(pml2, pml2e_t *, x86_pde_t *);
    return entry->present;
}

void platform_pml2e_set_present(pml2e_t *pml2, bool present)
{
    x86_pde_t *entry = cast_to(pml2, pml2e_t *, x86_pde_t *);
    entry->present = present;
}
void platform_pml2e_set_flags(pml2e_t *pml2, vm_flags flags)
{
    x86_pde_t *entry = cast_to(pml2, pml2e_t *, x86_pde_t *);
    entry->writable |= flags & VM_WRITE;
    entry->usermode |= flags & VM_USER;
    entry->write_through |= flags & VM_WRITE_THROUGH;
    entry->cache_disabled |= flags & VM_CACHE_DISABLED;
}
