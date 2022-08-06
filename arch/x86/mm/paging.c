// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/paging.h"

#include "lib/containers.h"
#include "lib/string.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/x86/boot/multiboot.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/x86_platform.h"

static const void *x86_paging_area_start = &__MOS_X86_PAGING_AREA_START;
static const void *x86_page_table_start = &__MOS_X86_PAGE_TABLE_START;
static const void *x86_paging_area_end = &__MOS_X86_PAGING_AREA_END;

pgdir_entry *mm_page_dir;
pgtable_entry *mm_page_table;

list_node_t mm_free_pages;

extern void x86_enable_paging_impl(void *page_dir);

void x86_mm_setup_paging()
{
    // validate if the memory region calculated from the linker script is correct.
    s64 paging_area_size = (uintptr_t) x86_paging_area_end - (uintptr_t) x86_paging_area_start;
    static const s64 paging_area_size_expected = 1024 * sizeof(pgdir_entry) + 1024 * 1024 * sizeof(pgtable_entry);
    pr_debug("paging: provided size: 0x%llx, minimum required size: 0x%llx", paging_area_size, paging_area_size_expected);
    MOS_ASSERT_X(paging_area_size >= paging_area_size_expected, "allocated paging area size is too small");

    // place the global page directory at somewhere outside of the kernel
    mm_page_dir = (pgdir_entry *) x86_paging_area_start;
    mm_page_table = (pgtable_entry *) x86_page_table_start;

    MOS_ASSERT_X((uintptr_t) mm_page_dir % 4096 == 0, "page directory is not aligned to 4096");
    MOS_ASSERT_X((uintptr_t) mm_page_table % 4096 == 0, "page table is not aligned to 4096");

    // initialize the page directory
    memset(mm_page_dir, 0, sizeof(pgdir_entry) * 1024);

    pr_debug("paging: setting up low 1MB identity mapping... (except the NULL page)");
    x86_mm_map_page(0, 0, PAGING_PRESENT); // ! the zero page is not writable
    for (int addr = X86_PAGE_SIZE; addr < 1 MB; addr += X86_PAGE_SIZE)
        x86_mm_map_page(addr, addr, PAGING_PRESENT | PAGING_WRITABLE);

    pr_debug("paging: mapping kernel space...");
    uintptr_t addr = (x86_kernel_start_addr / X86_PAGE_SIZE) * X86_PAGE_SIZE; // align the address to the page size
    while (addr < x86_kernel_end_addr)
    {
        x86_mm_map_page(addr, addr, PAGING_PRESENT | PAGING_WRITABLE);
        addr += X86_PAGE_SIZE;
    }
}

void x86_mm_map_page(uintptr_t vaddr, uintptr_t paddr, paging_entry_flags flags)
{
    // ensure the page is aligned to 4096
    MOS_ASSERT_X(vaddr % X86_PAGE_SIZE == 0, "vaddr is not aligned to 4096");

    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff;

    pgdir_entry *page_dir = mm_page_dir + page_dir_index;
    pgtable_entry *page_table;

    if (likely(page_dir->present))
    {
        page_table = ((pgtable_entry *) (page_dir->page_table_addr << 12)) + page_table_index;
    }
    else
    {
        page_table = mm_page_table + page_dir_index * 1024 + page_table_index;
        page_dir->present = true;
        page_dir->page_table_addr = (uintptr_t) page_table >> 12;
    }

    page_dir->writable |= !!(flags & PAGING_WRITABLE);
    page_dir->usermode |= !!(flags & PAGING_USERMODE);

    page_table->present = !!(flags & PAGING_PRESENT);
    page_table->writable = !!(flags & PAGING_WRITABLE);
    page_table->usermode = !!(flags & PAGING_USERMODE);
    page_table->phys_addr = (uintptr_t) paddr >> 12;
}

void x86_mm_unmap_page(uintptr_t vaddr)
{
    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff;

    pgdir_entry *page_dir = mm_page_dir + page_dir_index;
    if (unlikely(!page_dir->present))
    {
        mos_warn("page '%zx' not mapped", vaddr);
        return;
    }

    pgtable_entry *page_table = ((pgtable_entry *) (page_dir->page_table_addr << 12)) + page_table_index;
    page_table->present = false;
}

void x86_mm_enable_paging()
{
    pr_info("Page directory is at: %p", (void *) mm_page_dir);
    x86_enable_paging_impl(mm_page_dir);
    pr_info("Paging enabled.");
}

void *x86_mm_alloc_page(size_t n)
{
    MOS_UNUSED(n);
    return NULL;
}

bool x86_mm_free_page(void *vptr, size_t n)
{
    MOS_UNUSED(vptr);
    MOS_UNUSED(n);
    return false;
}
