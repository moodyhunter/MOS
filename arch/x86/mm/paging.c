// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/paging.h"

#include "lib/string.h"
#include "mos/mm/kmalloc.h"
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

void x86_mm_prepare_paging()
{
    // validate if the memory region calculated from the linker script is correct.
    size_t paging_area_size = (uintptr_t) x86_paging_area_end - (uintptr_t) x86_paging_area_start;
    mos_debug("paging: provided size: 0x%zu, minimum required size: 0x%zu", paging_area_size, sizeof(x86_pg_infra_t));
    MOS_ASSERT_X(paging_area_size >= sizeof(x86_pg_infra_t), "allocated paging area size is too small");

    // place the global page directory at somewhere outside of the kernel
    x86_pg_infra_t *kpg_infra = (x86_pg_infra_t *) x86_paging_area_start;
    *((uintptr_t *) &mos_platform.kernel_pg.ptr) = (uintptr_t) kpg_infra;

    MOS_ASSERT_X((uintptr_t) kpg_infra->pgdir % (4 KB) == 0, "page directory is not aligned to 4KB");
    MOS_ASSERT_X((uintptr_t) kpg_infra->pgtable % (4 KB) == 0, "page table is not aligned to 4KB");

    // initialize the page directory
    memset(kpg_infra, 0, sizeof(x86_pg_infra_t));

    pr_info("paging: setting up physical memory freelist...");
    pmem_freelist_setup(kpg_infra);

    pr_info("paging: setting up low 1MB identity mapping... (except for the NULL page)");
    // skip the free list setup, use do_ version
    pg_do_map_page(kpg_infra, 0, 0, VM_NONE); // ! the zero page is not writable
    pg_do_map_pages(kpg_infra, X86_PAGE_SIZE, X86_PAGE_SIZE, 1 MB / X86_PAGE_SIZE, VM_WRITABLE);

    pr_info("paging: mapping kernel space...");
    const uintptr_t kstart_aligned = X86_ALIGN_DOWN_TO_PAGE(x86_kernel_start);
    const uintptr_t kend_aligned = X86_ALIGN_UP_TO_PAGE(x86_kernel_end);
    pg_map_pages(kpg_infra, kstart_aligned, kstart_aligned, (kend_aligned - kstart_aligned) / X86_PAGE_SIZE, VM_WRITABLE);
}

void x86_mm_enable_paging(x86_pg_infra_t *kpg_infra)
{
    pr_info("paging: converting physical memory freelist to vm mode");
    pmem_freelist_convert_to_vm();

    pr_info("paging: page directory at: %p", (void *) kpg_infra->pgdir);
    x86_enable_paging_impl(kpg_infra->pgdir);
    pr_info("paging: Enabled");
}

void x86_mm_dump_page_table(x86_pg_infra_t *pg)
{
    static const s32 NONE = -1;
    pr_info("paging: dumping page table");

    s32 present_begin = NONE;
    s32 absent_begin = NONE;

    for (int pgd_i = 0; pgd_i < 1024; pgd_i++)
    {
        x86_pgdir_entry *pgd = pg->pgdir + pgd_i;
        if (!pgd->present)
        {
            if (absent_begin == NONE)
                absent_begin = pgd_i * 1024;
            continue;
        }

        for (int pgt_i = 0; pgt_i < 1024; pgt_i++)
        {
            uintptr_t table_id = pgd_i * 1024 + pgt_i;
            x86_pgtable_entry *pgt = pg->pgtable + table_id;
            if (pgt->present)
            {
                if (present_begin == NONE)
                    present_begin = table_id;
                if (absent_begin != NONE)
                {
                    pr_info("  [" PTR_FMT "-" PTR_FMT "] not present", (uintptr_t) absent_begin * X86_PAGE_SIZE, (table_id - 1) * X86_PAGE_SIZE);
                    absent_begin = NONE;
                }
            }
            else
            {
                if (absent_begin == NONE)
                    absent_begin = table_id;
                if (present_begin != NONE)
                {
                    pr_info("  [" PTR_FMT "-" PTR_FMT "] present", (uintptr_t) present_begin * X86_PAGE_SIZE, (table_id - 1) * X86_PAGE_SIZE);
                    present_begin = NONE;
                }
            }
        }
    }

    if (present_begin != NONE)
        pr_info("  [" PTR_FMT "-" PTR_FMT "] present", (uintptr_t) present_begin * X86_PAGE_SIZE, (uintptr_t) -1);
    if (absent_begin != NONE)
        pr_info("  [" PTR_FMT "-" PTR_FMT "] not present", (uintptr_t) absent_begin * X86_PAGE_SIZE, (uintptr_t) -1);
}

void x86_um_pgd_init(paging_handle_t *pgt)
{
    pgt->ptr = (uintptr_t) kmalloc(sizeof(x86_pg_infra_t));
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
