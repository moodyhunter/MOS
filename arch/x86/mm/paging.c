// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/paging.h"

#include "lib/string.h"
#include "mos/mm/paging/paging.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/x86/mm/paging_impl.h"
#include "mos/x86/x86_platform.h"

// defined in enable_paging.asm
extern void x86_enable_paging_impl(uintptr_t page_dir);

static x86_pg_infra_t x86_kpg_infra_storage __aligned(MOS_PAGE_SIZE) = { 0 };
static spinlock_t x86_kernel_pgd_lock = SPINLOCK_INIT;

x86_pg_infra_t *const x86_kpg_infra = &x86_kpg_infra_storage;

void x86_mm_paging_init()
{
    // initialize the page directory
    memzero(x86_kpg_infra, sizeof(x86_pg_infra_t));
    x86_platform.kernel_pgd.pgd = (uintptr_t) x86_kpg_infra;
    x86_platform.kernel_pgd.um_page_map = NULL; // a kernel page table does not have a user-mode page map
    x86_platform.kernel_pgd.pgd_lock = &x86_kernel_pgd_lock;
    current_cpu->pagetable = x86_platform.kernel_pgd;
}

void x86_mm_enable_paging(void)
{
    mos_debug(x86_paging, "page directory vaddr at: %p", (void *) x86_kpg_infra->pgdir);
    x86_enable_paging_impl(((uintptr_t) x86_kpg_infra->pgdir) - MOS_KERNEL_START_VADDR);
    pr_info("paging: enabled");

#if MOS_DEBUG_FEATURE(x86_paging)
    x86_mm_dump_page_table(x86_kpg_infra);
#endif
}

void page_table_dump_range(x86_pg_infra_t *kpg_infra, size_t start_page, size_t end_page)
{
    pr_info("  VM Group " PTR_FMT " (+%zu pages):", (uintptr_t) start_page * MOS_PAGE_SIZE, end_page - start_page);

    size_t i = start_page;
    while (true)
    {
        if (i >= end_page)
            return;

        x86_pgtable_entry *pgtable = kpg_infra->pgtable + i;
        MOS_ASSERT(pgtable->present);

        uintptr_t p_range_start = pgtable->phys_addr << 12;
        uintptr_t p_range_last = p_range_start;

        uintptr_t v_range_start = i * MOS_PAGE_SIZE;

        bool range_begin_usermode = pgtable->usermode;
        bool range_begin_writable = pgtable->writable;

        // ! Special case: end_page - start_page == 1
        if (end_page - start_page == 1)
        {
            uintptr_t v_range_end = v_range_start + MOS_PAGE_SIZE;
            uintptr_t p_range_end = p_range_start + MOS_PAGE_SIZE;
            pr_info("    " PTR_FMT "-" PTR_FMT " -> " PTR_FMT "-" PTR_FMT " (%s, %s)", v_range_start, v_range_end, p_range_start, p_range_end,
                    range_begin_writable ? "rw" : "ro", range_begin_usermode ? "user" : "kernel");
            return;
        }

        while (true)
        {
            i++;
            if (i >= end_page)
                return;

            x86_pgtable_entry *this_t = kpg_infra->pgtable + i;
            MOS_ASSERT(this_t->present);

            uintptr_t p_range_current = this_t->phys_addr << 12;

            bool is_last_page = i == end_page - 1;
            bool is_continuous = p_range_current == p_range_last + MOS_PAGE_SIZE && this_t->usermode == range_begin_usermode && this_t->writable == range_begin_writable;

            if (is_continuous && !is_last_page)
            {
                p_range_last = p_range_current;
                continue;
            }

            uintptr_t v_range_end = i * MOS_PAGE_SIZE;
            uintptr_t p_range_end = p_range_current + MOS_PAGE_SIZE;
            // '!is_continuous' case : print the previous range, but add one page
            if (!is_continuous)
                p_range_end = p_range_last + MOS_PAGE_SIZE;
            else
                v_range_end += MOS_PAGE_SIZE;
            MOS_ASSERT(p_range_end - p_range_start == v_range_end - v_range_start);
            pr_info("    " PTR_FMT "-" PTR_FMT " -> " PTR_FMT "-" PTR_FMT " (%s, %s)", v_range_start, v_range_end, p_range_start, p_range_end,
                    range_begin_writable ? "rw" : "ro", range_begin_usermode ? "user" : "kernel");
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

        x86_pgtable_entry *pgtable = NULL;
        x86_pg_infra_t *dump_pg = NULL;
        if (pgd_i < 768) // user pages
            pgtable = pg->pgtable, dump_pg = pg;
        else // HACK: kernel pages
            pgtable = x86_kpg_infra->pgtable, dump_pg = x86_kpg_infra;

        for (int pte_i = 0; pte_i < 1024; pte_i++)
        {
            x86_pgtable_entry *pte = &pgtable[pgd_i * 1024 + pte_i];

            if (!pte->present)
            {
                if (current_state == PRESENT)
                {
                    size_t current_state_end_pg = pgd_i * 1024 + pte_i;
                    page_table_dump_range(dump_pg, current_state_begin_pg, current_state_end_pg);
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
