// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/paging.h"

#include "lib/stdlib.h"
#include "lib/string.h"
#include "mos/kconfig.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/mm_types.h"
#include "mos/mm/paging.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/types.h"
#include "mos/x86/acpi/acpi.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/mm/pmem_freelist.h"
#include "mos/x86/x86_platform.h"

// defined in enable_paging.asm
extern void x86_enable_paging_impl(void *page_dir);

static const void *x86_paging_area_start = &__MOS_X86_PAGING_AREA_START;
static const void *x86_paging_area_end = &__MOS_X86_PAGING_AREA_END;
x86_pg_infra_t *x86_kpg_infra = (void *) &__MOS_X86_PAGING_AREA_START;

void x86_mm_prepare_paging()
{
    // validate if the memory region calculated from the linker script is correct.
    size_t paging_area_size = (uintptr_t) x86_paging_area_end - (uintptr_t) x86_paging_area_start;
    MOS_ASSERT_X(paging_area_size >= sizeof(x86_pg_infra_t), "allocated paging area size is too small");
    MOS_ASSERT_X((uintptr_t) x86_kpg_infra->pgdir % (4 KB) == 0, "page directory is not aligned to 4KB");
    MOS_ASSERT_X((uintptr_t) x86_kpg_infra->pgtable % (4 KB) == 0, "page table is not aligned to 4KB");

    mos_debug("paging: provided size: 0x%zu, minimum required size: 0x%zu", paging_area_size, sizeof(x86_pg_infra_t));

    // initialize the page directory
    memset(x86_kpg_infra, 0, sizeof(x86_pg_infra_t));

    pr_info("paging: setting up physical memory freelist...");
    pmem_freelist_setup(x86_kpg_infra);

    pr_info("paging: setting up low 1MB identity mapping... (except for the NULL page)");
    // skip the free list setup, use do_ version
    pg_do_map_page(x86_kpg_infra, 0, 0, VM_NONE); // ! the zero page is not writable
    pg_do_map_pages(x86_kpg_infra, X86_PAGE_SIZE, X86_PAGE_SIZE, 1 MB / X86_PAGE_SIZE - 1, VM_GLOBAL | VM_WRITE);

    pr_info("paging: mapping kernel space...");
    const uintptr_t k_rostart = (uintptr_t) &__MOS_KERNEL_RO_START;
    const uintptr_t k_roend = (uintptr_t) &__MOS_KERNEL_RO_END;
    pg_map_pages(x86_kpg_infra, k_rostart, k_rostart, (k_roend - k_rostart) / X86_PAGE_SIZE, VM_GLOBAL);

    const uintptr_t k_rwstart = (uintptr_t) &__MOS_KERNEL_RW_START;
    const uintptr_t k_rwend = (uintptr_t) &__MOS_KERNEL_RW_END;
    pg_map_pages(x86_kpg_infra, k_rwstart, k_rwstart, (k_rwend - k_rwstart) / X86_PAGE_SIZE, VM_GLOBAL | VM_WRITE);
}

void x86_mm_enable_paging(x86_pg_infra_t *kpg_infra)
{
    mos_debug("paging: converting physical memory freelist to vm mode");
    pmem_freelist_convert_to_vm();

    mos_debug("paging: page directory at: %p", (void *) kpg_infra->pgdir);
    x86_enable_paging_impl(kpg_infra->pgdir);
    pr_info("paging: enabled");

#if MOS_DEBUG
    x86_mm_dump_page_table(kpg_infra);
    pmem_freelist_dump();
#endif
}

void page_table_dump_range(x86_pg_infra_t *kpg_infra, size_t start_page, size_t end_page)
{
    pr_info("  VM Group " PTR_FMT " (+%zu pages):", (uintptr_t) start_page * X86_PAGE_SIZE, end_page - start_page);

    size_t i = start_page;
    while (true)
    {
        if (i >= end_page)
            return;

        x86_pgtable_entry *pgtable = kpg_infra->pgtable + i;
        MOS_ASSERT(pgtable->present);

        uintptr_t p_range_start = pgtable->phys_addr << 12;
        uintptr_t p_range_last = p_range_start;

        uintptr_t v_range_start = i * X86_PAGE_SIZE;

        // ! Special case: end_page - start_page == 1
        if (end_page - start_page == 1)
        {
            uintptr_t v_range_end = v_range_start + X86_PAGE_SIZE;
            uintptr_t p_range_end = p_range_start + X86_PAGE_SIZE;
            pr_info("    " PTR_FMT "-" PTR_FMT " -> " PTR_FMT "-" PTR_FMT, v_range_start, v_range_end, p_range_start, p_range_end);
            return;
        }

        while (true)
        {
            i++;
            if (i >= end_page)
                return;

            x86_pgtable_entry *this_entry = kpg_infra->pgtable + i;
            MOS_ASSERT(this_entry->present);

            uintptr_t p_range_current = this_entry->phys_addr << 12;

            bool is_last_page = i == end_page - 1;
            bool is_continuous = p_range_current == p_range_last + X86_PAGE_SIZE;

            if (is_continuous && !is_last_page)
            {
                p_range_last = p_range_current;
                continue;
            }

            uintptr_t v_range_end = i * X86_PAGE_SIZE;
            uintptr_t p_range_end = p_range_current + X86_PAGE_SIZE;
            // '!is_continuous' case : print the previous range, but add one page
            if (!is_continuous)
                p_range_end = p_range_last + X86_PAGE_SIZE;
            else
                v_range_end += X86_PAGE_SIZE;
            MOS_ASSERT(p_range_end - p_range_start == v_range_end - v_range_start);
            pr_info("    " PTR_FMT "-" PTR_FMT " -> " PTR_FMT "-" PTR_FMT, v_range_start, v_range_end, p_range_start, p_range_end);
            break;
        }
    }
}

void x86_mm_dump_page_table(x86_pg_infra_t *pg)
{
    pr_info("paging: dumping page table");
    enum
    {
        PRESENT,
        ABSENT
    } current_state = pg->pgtable[0].present ? PRESENT : ABSENT;

    size_t current_state_begin_pg = 0;

    for (int pgd_i = 0; pgd_i < 1024; pgd_i++)
    {
        x86_pgdir_entry *pgd = pg->pgdir + pgd_i;
        if (!pgd->present)
        {
            if (current_state == PRESENT)
            {
                size_t current_state_end_pg = pgd_i * 1024 + 0;
                page_table_dump_range(pg, current_state_begin_pg, current_state_end_pg);
                current_state = ABSENT;
                current_state_begin_pg = current_state_end_pg;
            }
            continue;
        }

        for (int pte_i = 0; pte_i < 1024; pte_i++)
        {
            x86_pgtable_entry *pte = &pg->pgtable[pgd_i * 1024 + pte_i];
            if (!pte->present)
            {
                if (current_state == PRESENT)
                {
                    size_t current_state_end_pg = pgd_i * 1024 + pte_i;
                    page_table_dump_range(pg, current_state_begin_pg, current_state_end_pg);
                    current_state = ABSENT;
                    current_state_begin_pg = current_state_end_pg;
                }
            }
            else
            {
                if (current_state == ABSENT)
                {
                    current_state = PRESENT;
                    current_state_begin_pg = pgd_i * 1024 + pte_i;
                }
            }
        }
    }
}

paging_handle_t x86_um_pgd_create()
{
    x86_pg_infra_t *infra = pg_page_alloc(x86_kpg_infra, X86_ALIGN_UP_TO_PAGE(sizeof(x86_pg_infra_t)) / X86_PAGE_SIZE, PGALLOC_NONE);
    memset(infra, 0, sizeof(x86_pg_infra_t));
    paging_handle_t handle;
    handle.ptr = (uintptr_t) infra;

    const size_t k_ro_prange = (mos_platform->regions.ro_end - mos_platform->regions.ro_start) / X86_PAGE_SIZE;
    const size_t k_rw_prange = (mos_platform->regions.rw_end - mos_platform->regions.rw_start) / X86_PAGE_SIZE;

    // pg_do_map_pages(infra, 0, 0, 1, VM_NONE); // ! the zero page is not writable, nor readable by user
    pg_do_map_pages(infra, X86_PAGE_SIZE, X86_PAGE_SIZE, 1 MB / X86_PAGE_SIZE - 1, VM_GLOBAL | VM_WRITE);
    pg_do_map_pages(infra, mos_platform->regions.ro_start, mos_platform->regions.ro_start, k_ro_prange, VM_GLOBAL);
    pg_do_map_pages(infra, mos_platform->regions.rw_start, mos_platform->regions.rw_start, k_rw_prange, VM_GLOBAL | VM_WRITE);

    memblock_t *bios_block = x86_mem_find_bios_block();
    pg_do_map_pages(infra, bios_block->paddr, bios_block->paddr, bios_block->size_bytes / X86_PAGE_SIZE, VM_GLOBAL);

    // map the lapic to the kernel page table
    pg_do_map_pages(infra, x86_acpi_madt->lapic_addr, x86_acpi_madt->lapic_addr, 1, VM_GLOBAL);

    // map kernel heap
    // this is a bit of a hack, but it's the easiest way that I can think of ...
    const int heap_pagedir = MOS_X86_HEAP_BASE_VADDR / (1024 * X86_PAGE_SIZE);
    for (int i = heap_pagedir; i < 1024; i++)
    {
        x86_pgdir_entry *pgd = &infra->pgdir[i];
        pgd->present = true;
        pgd->writable = true;
        pgd->usermode = false;
        // redirect it to the kernel page table
        pgd->page_table_paddr = (uintptr_t) x86_kpg_infra->pgdir[i].page_table_paddr;
    }
    return handle;
}

void x86_um_pgd_destroy(paging_handle_t pgt)
{
    kfree((void *) pgt.ptr);
}

void *x86_mm_pg_alloc(paging_handle_t pgt, size_t n, pagealloc_flags flags)
{
    x86_pg_infra_t *kpg_infra = x86_get_pg_infra(pgt);
    return pg_page_alloc(kpg_infra, n, flags);
}

bool x86_mm_pg_free(paging_handle_t pgt, uintptr_t vaddr, size_t n)
{
    x86_pg_infra_t *kpg_infra = x86_get_pg_infra(pgt);
    return pg_page_free(kpg_infra, vaddr, n);
}

void x86_mm_pg_flag(paging_handle_t pgt, uintptr_t vaddr, size_t n, vm_flags flags)
{
    x86_pg_infra_t *kpg_infra = x86_get_pg_infra(pgt);
    pg_page_flag(kpg_infra, vaddr, n, flags);
}

void x86_mm_pg_map_to_kvirt(paging_handle_t table, uintptr_t vaddr, uintptr_t kvaddr, size_t n, vm_flags flags)
{
    x86_pg_infra_t *pg_infra = x86_get_pg_infra(table);
    uintptr_t paddr = pg_page_get_mapped_paddr(x86_kpg_infra, kvaddr);
    pg_do_map_pages(pg_infra, vaddr, paddr, n, flags);
}

void x86_mm_pg_unmap(paging_handle_t table, uintptr_t vaddr, size_t n)
{
    x86_pg_infra_t *pg_infra = x86_get_pg_infra(table);
    pg_do_unmap_pages(pg_infra, vaddr, n);
}
