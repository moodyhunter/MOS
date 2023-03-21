// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/printk.h>
#include <mos/x86/cpu/cpu.h>
#include <mos/x86/mm/paging.h>
#include <mos/x86/mm/paging_impl.h>
#include <mos/x86/x86_platform.h>

#define PAGING_CORRECT_PGTABLE_SANITY_CHECKS(_pg, _vaddr)                                                                                                                \
    do                                                                                                                                                                   \
    {                                                                                                                                                                    \
        if (_pg == x86_kpg_infra)                                                                                                                                        \
            MOS_ASSERT_X(_vaddr >= MOS_KERNEL_START_VADDR, "operating with [userspace address] in the [kernel page] table");                                             \
        else                                                                                                                                                             \
            MOS_ASSERT_X(_vaddr < MOS_KERNEL_START_VADDR, "operating with [kernel address] in the [userspace page] table");                                              \
    } while (0)

void pg_flag_page(x86_pg_infra_t *pg, uintptr_t vaddr, size_t n, vm_flags flags)
{
    MOS_ASSERT_X(vaddr % MOS_PAGE_SIZE == 0, "vaddr is not aligned to 4096");
    PAGING_CORRECT_PGTABLE_SANITY_CHECKS(pg, vaddr);

    size_t start_page = vaddr / MOS_PAGE_SIZE;
    for (size_t i = 0; i < n; i++)
    {
        size_t page_i = start_page + i;
        size_t pgd_i = page_i / 1024;

        MOS_ASSERT_X(pg->pgdir[pgd_i].present, "page directory not present");
        MOS_ASSERT_X(pg->pgtable[page_i].present, "page table not present");

        pg->pgdir[pgd_i].writable |= flags & VM_WRITE;
        pg->pgtable[page_i].writable = flags & VM_WRITE;

        pg->pgdir[pgd_i].usermode |= flags & VM_USER;
        pg->pgtable[page_i].usermode = flags & VM_USER;

        pg->pgdir[pgd_i].cache_disabled |= flags & VM_CACHE_DISABLED;
        pg->pgtable[page_i].cache_disabled = flags & VM_CACHE_DISABLED;

        pg->pgdir[pgd_i].write_through |= flags & VM_WRITE_THROUGH;
        pg->pgtable[page_i].write_through = flags & VM_WRITE_THROUGH;

        pg->pgtable[page_i].global = flags & VM_GLOBAL;
        x86_cpu_invlpg(vaddr);
    }
}

void pg_map_page(x86_pg_infra_t *pg, uintptr_t vaddr, uintptr_t paddr, vm_flags flags)
{
    MOS_ASSERT_X(vaddr % MOS_PAGE_SIZE == 0, "vaddr is not aligned to page size");
    MOS_ASSERT_X(paddr % MOS_PAGE_SIZE == 0, "paddr is not aligned to page size");
    PAGING_CORRECT_PGTABLE_SANITY_CHECKS(pg, vaddr);

    const u32 pd_index = vaddr >> 22;
    const u32 pt_index = pd_index * 1024 + (vaddr >> 12 & 0x3ff);

    x86_pgdir_entry *this_dir = &pg->pgdir[pd_index];
    x86_pgtable_entry *this_table = &pg->pgtable[pt_index];

    if (unlikely(!this_dir->present))
    {
        this_dir->present = true;

        if (vaddr >= MOS_KERNEL_START_VADDR)
        {
            // kernel page tables are identity mapped, directly subtract the kernel start address
            // chicken and egg problem: get the physical address of the very first page table
            this_dir->page_table_paddr = ((uintptr_t) &pg->pgtable[pd_index * 1024] - MOS_KERNEL_START_VADDR) >> 12;
        }
        else
        {
            this_dir->page_table_paddr = pg_get_mapped_paddr(x86_kpg_infra, (uintptr_t) this_table) >> 12;
        }
    }

    MOS_ASSERT_X(this_table->present == false, "page " PTR_FMT " already mapped", vaddr);

    this_table->present = true;
    this_table->phys_addr = (uintptr_t) paddr >> 12;

    this_dir->writable |= flags & VM_WRITE;
    this_table->writable = flags & VM_WRITE;

    this_dir->usermode |= flags & VM_USER;
    this_table->usermode = flags & VM_USER;

    this_dir->cache_disabled |= flags & VM_CACHE_DISABLED;
    this_table->cache_disabled = flags & VM_CACHE_DISABLED;

    this_table->global = flags & VM_GLOBAL;

    x86_cpu_invlpg(vaddr);
}

void pg_unmap_page(x86_pg_infra_t *pg, uintptr_t vaddr)
{
    MOS_ASSERT_X(vaddr % MOS_PAGE_SIZE == 0, "vaddr is not aligned to 4096");
    PAGING_CORRECT_PGTABLE_SANITY_CHECKS(pg, vaddr);

    const u32 pd_index = vaddr >> 22;
    const u32 pt_index = pd_index * 1024 + (vaddr >> 12 & 0x3ff);

    x86_pgdir_entry *page_dir = &pg->pgdir[pd_index];
    if (unlikely(!page_dir->present))
    {
        mos_panic("vmem " PTR_FMT " not mapped", vaddr);
        return;
    }

    pg->pgtable[pt_index].present = false;
    x86_cpu_invlpg(vaddr);
}

// !! TODO: read real address instead of assuming memory layout = x86_pg_infra_t
uintptr_t pg_get_mapped_paddr(x86_pg_infra_t *pg, uintptr_t vaddr)
{
    PAGING_CORRECT_PGTABLE_SANITY_CHECKS(pg, vaddr);

    size_t page_dir_index = vaddr >> 22;
    size_t page_table_index = vaddr >> 12 & 0x3ff;
    x86_pgdir_entry *page_dir = pg->pgdir + page_dir_index;
    x86_pgtable_entry *page_table = pg->pgtable + page_dir_index * 1024 + page_table_index;

    if (vaddr >= MOS_KERNEL_START_VADDR)
        page_dir = x86_kpg_infra->pgdir + page_dir_index, page_table = x86_kpg_infra->pgtable + page_dir_index * 1024 + page_table_index;

    if (unlikely(!page_dir->present))
        mos_panic("page directory for address " PTR_FMT " not mapped", vaddr);

    if (unlikely(!page_table->present))
        mos_panic("vmem " PTR_FMT " not mapped", vaddr);

    return (page_table->phys_addr << 12) + (vaddr & 0xfff);
}

vm_flags pg_get_flags(x86_pg_infra_t *pg, uintptr_t vaddr)
{
    MOS_ASSERT_X(vaddr % MOS_PAGE_SIZE == 0, "vaddr is not aligned to 4096");
    PAGING_CORRECT_PGTABLE_SANITY_CHECKS(pg, vaddr);

    size_t page_dir_index = vaddr >> 22;
    size_t page_table_index = vaddr >> 12 & 0x3ff;
    x86_pgdir_entry *page_dir = pg->pgdir + page_dir_index;
    x86_pgtable_entry *page_table = pg->pgtable + page_dir_index * 1024 + page_table_index;

    if (vaddr >= MOS_KERNEL_START_VADDR)
        page_dir = x86_kpg_infra->pgdir + page_dir_index, page_table = x86_kpg_infra->pgtable + page_dir_index * 1024 + page_table_index;

    if (unlikely(!page_dir->present))
        mos_panic("page directory for address " PTR_FMT " not mapped", vaddr);

    if (unlikely(!page_table->present))
        mos_panic("vmem " PTR_FMT " not mapped", vaddr);

    vm_flags flags = VM_READ;
    flags |= (page_dir->writable && page_table->writable) ? VM_WRITE : 0;
    flags |= (page_dir->usermode && page_table->usermode) ? VM_USER : 0;
    flags |= (page_dir->cache_disabled && page_table->cache_disabled) ? VM_CACHE_DISABLED : 0;
    flags |= page_table->global ? VM_GLOBAL : 0;
    return flags;
}
