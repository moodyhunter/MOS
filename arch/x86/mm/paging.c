// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/paging.h"

#include "lib/stdlib.h"
#include "lib/string.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/types.h"
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
    pg_do_map_pages(x86_kpg_infra, X86_PAGE_SIZE, X86_PAGE_SIZE, 1 MB / X86_PAGE_SIZE - 1, VM_WRITABLE);

    pr_info("paging: mapping kernel space...");
    const uintptr_t kstart_aligned = MAX(x86_kernel_start, 1 MB);
    const uintptr_t kend_aligned = X86_ALIGN_UP_TO_PAGE(x86_kernel_end);
    pg_map_pages(x86_kpg_infra, kstart_aligned, kstart_aligned, (kend_aligned - kstart_aligned) / X86_PAGE_SIZE, VM_WRITABLE);
}

void x86_mm_enable_paging(x86_pg_infra_t *kpg_infra)
{
    mos_debug("paging: converting physical memory freelist to vm mode");
    pmem_freelist_convert_to_vm();

    mos_debug("paging: page directory at: %p", (void *) kpg_infra->pgdir);
    x86_enable_paging_impl(kpg_infra->pgdir);
    pr_info("paging: enabled");

#if MOS_ENABLE_DEBUG_LOG
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
    typedef enum
    {
        PRESENT,
        NPRESENT
    } state_t;
    state_t current_state = pg->pgtable[0].present ? PRESENT : NPRESENT;

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
                current_state = NPRESENT;
                current_state_begin_pg = current_state_end_pg;
            }
            continue;
        }

        for (int pte_i = 0; pte_i < 1024; pte_i++)
        {
            x86_pgtable_entry *pte = (x86_pgtable_entry *) (pgd->page_table_addr << 12) + pte_i;
            if (!pte->present)
            {
                if (current_state == PRESENT)
                {
                    size_t current_state_end_pg = pgd_i * 1024 + pte_i;
                    page_table_dump_range(pg, current_state_begin_pg, current_state_end_pg);
                    current_state = NPRESENT;
                    current_state_begin_pg = current_state_end_pg;
                }
            }
            else
            {
                if (current_state == NPRESENT)
                {
                    current_state = PRESENT;
                    current_state_begin_pg = pgd_i * 1024 + pte_i;
                }
            }
        }
    }
}

void x86_um_pgd_init(paging_handle_t *pgt)
{
    void *addr = pg_page_alloc(x86_kpg_infra, X86_ALIGN_UP_TO_PAGE(sizeof(x86_pg_infra_t)) / X86_PAGE_SIZE);
    memset(addr, 0, sizeof(x86_pg_infra_t));
    pgt->ptr = (uintptr_t) addr;
}

void x86_um_pgd_deinit(paging_handle_t pgt)
{
    kfree((void *) pgt.ptr);
}

void *x86_mm_pg_alloc(paging_handle_t pgt, size_t n)
{
    x86_pg_infra_t *kpg_infra = x86_get_pg_infra(pgt);
    return pg_page_alloc(kpg_infra, n);
}

bool x86_mm_pg_free(paging_handle_t pgt, uintptr_t vaddr, size_t n)
{
    x86_pg_infra_t *kpg_infra = x86_get_pg_infra(pgt);
    return pg_page_free(kpg_infra, vaddr, n);
}

void x86_mm_pg_flag(paging_handle_t pgt, uintptr_t vaddr, size_t n, page_flags flags)
{
    x86_pg_infra_t *kpg_infra = x86_get_pg_infra(pgt);
    pg_page_flag(kpg_infra, vaddr, n, flags);
}

void x86_mm_pg_map_to_kvirt(paging_handle_t table, uintptr_t vaddr, uintptr_t kvaddr, size_t n, page_flags flags)
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
