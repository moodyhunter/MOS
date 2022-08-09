// SPDX-Licence-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/paging.h"

#include "lib/stdlib.h"
#include "lib/string.h"
#include "mos/kconfig.h"
#include "mos/printk.h"
#include "mos/x86/mm/mm.h"
#include "mos/x86/x86_platform.h"

#define PAGEMAP_WIDTH    (sizeof(u32) * 8)
#define MM_PAGE_MAP_SIZE (X86_MAX_MEM_SIZE / X86_PAGE_SIZE / PAGEMAP_WIDTH)
static u32 mm_page_map[MM_PAGE_MAP_SIZE] = { 0 };

#define PAGEMAP_MAP(index)   mm_page_map[index / PAGEMAP_WIDTH] |= 1 << (index % PAGEMAP_WIDTH)
#define PAGEMAP_UNMAP(index) mm_page_map[index / PAGEMAP_WIDTH] &= ~(1 << (index % PAGEMAP_WIDTH))

extern void x86_enable_paging_impl(void *page_dir);

static const void *x86_paging_area_start = &__MOS_X86_PAGING_AREA_START;
static const void *x86_page_table_start = &__MOS_X86_PAGE_TABLE_START;
static const void *x86_paging_area_end = &__MOS_X86_PAGING_AREA_END;

static pgdir_entry *mm_page_dir;
static pgtable_entry *mm_page_table;

typedef struct pmem_range_t
{
    struct pmem_range_t *next;
    u32 paddr;
    size_t n_pages;
} pmem_range_t;

#define PMEM_FREELIST_SIZE_FOR(mem_size) ((mem_size / 2 / X86_PAGE_SIZE) * sizeof(pmem_range_t))

#define PMEM_FREELIST_VADDR 0xFFA00000

// The list holding free memory ranges, this list terminates with [an entry with next == NULL]
static uint32_t pmem_freelist_base_paddr;
static pmem_range_t *pmem_freelist_head = NULL;
static size_t pmem_freelist_count = 0;

static_assert((u64) PMEM_FREELIST_VADDR + PMEM_FREELIST_SIZE_FOR(X86_MAX_MEM_SIZE) < (u64) X86_MAX_MEM_SIZE, "no enough vspace for freelist!");

void x86_mm_prepare_paging()
{
    // validate if the memory region calculated from the linker script is correct.
    s64 paging_area_size = (uintptr_t) x86_paging_area_end - (uintptr_t) x86_paging_area_start;
    static const s64 paging_area_size_expected = 1024 * sizeof(pgdir_entry) + 1024 * 1024 * sizeof(pgtable_entry);
    mos_debug("paging: provided size: 0x%llx, minimum required size: 0x%llx", paging_area_size, paging_area_size_expected);
    MOS_ASSERT_X(paging_area_size >= paging_area_size_expected, "allocated paging area size is too small");

    // place the global page directory at somewhere outside of the kernel
    mm_page_dir = (pgdir_entry *) x86_paging_area_start;
    mm_page_table = (pgtable_entry *) x86_page_table_start;

    MOS_ASSERT_X((uintptr_t) mm_page_dir + 1024 * sizeof(pgtable_entry) <= (uintptr_t) mm_page_table, "overlap between page table & directory");
    MOS_ASSERT_X((uintptr_t) mm_page_dir % X86_PAGE_SIZE == 0, "page directory is not aligned to 4096");
    MOS_ASSERT_X((uintptr_t) mm_page_table % X86_PAGE_SIZE == 0, "page table is not aligned to 4096");

    // initialize the page directory
    memset(mm_page_dir, 0, sizeof(pgdir_entry) * 1024);

    mos_debug("paging: setting up physical memory freelist...");
    pmem_freelist_setup();

    mos_debug("paging: setting up low 1MB identity mapping... (except the NULL page)");
    vm_map_page_range(0, 0, 1, PAGING_PRESENT);
    vm_map_page_range(X86_PAGE_SIZE, X86_PAGE_SIZE, 1 MB / X86_PAGE_SIZE, PAGING_PRESENT | PAGING_WRITABLE);

    mos_debug("paging: mapping kernel space...");
    vm_map_page_range(x86_kernel_start, x86_kernel_start, (x86_kernel_end - x86_kernel_start) / X86_PAGE_SIZE, PAGING_PRESENT | PAGING_WRITABLE);
}

void x86_mm_enable_paging()
{
    pr_info("paging: converting physical memory freelist to vm mode");
    pmem_range_t *this = pmem_freelist_head;
    while (this->next)
    {
        pmem_range_t *pnext = this->next;
        this->next = (pmem_range_t *) (PMEM_FREELIST_VADDR + (uintptr_t) pnext - pmem_freelist_base_paddr);
        this = pnext;
    }
    pmem_freelist_head = (pmem_range_t *) (PMEM_FREELIST_VADDR + (uintptr_t) pmem_freelist_head - pmem_freelist_base_paddr);

    pr_info("paging: page directory at: %p", (void *) mm_page_dir);
    x86_enable_paging_impl(mm_page_dir);
    pr_info("paging: Enabled");
}

void *x86_mm_alloc_page(size_t n_page)
{
#define BIT_IS_SET(byte, bit) ((byte) & (1 << (bit)))
    // simply rename the variable, we are dealing with bitmaps
    size_t n_bits = n_page;
    size_t n_zero_bits = 0;

    u8 target_bit = 0;

    // always allocate after the end of the kernel
    size_t kernel_page_end = x86_kernel_end / X86_PAGE_SIZE;
    size_t target_pagemap_start = (kernel_page_end / PAGEMAP_WIDTH) + 1;

    for (size_t i = target_pagemap_start; n_zero_bits < n_bits; i++)
    {
        if (i >= MM_PAGE_MAP_SIZE)
        {
            mos_warn("failed to allocate %zu pages", n_page);
            return NULL;
        }
        u8 current_byte = mm_page_map[i];

        if (current_byte == 0)
        {
            n_zero_bits += PAGEMAP_WIDTH;
            continue;
        }
        if (current_byte == 0xff)
        {
            target_pagemap_start = i + 1;
            continue;
        }

        for (u32 bit = 0; bit < PAGEMAP_WIDTH; bit++)
        {
            if (!BIT_IS_SET(current_byte, bit))
                n_zero_bits++;
            else
                n_zero_bits = 0, target_bit = bit + 1, target_pagemap_start = i;
        }
    }

    size_t page_index = target_pagemap_start * PAGEMAP_WIDTH + target_bit;
    mos_debug("paging: allocating %zu to %zu", page_index, page_index + n_page);
    void *vaddr = (void *) (page_index * X86_PAGE_SIZE);

    // !! id map the page (for now)
    mos_warn_once("x86 paging is not implemented correctly, id mapping is used");
    vm_map_page_range((uintptr_t) vaddr, (uintptr_t) vaddr, n_page, PAGING_PRESENT | PAGING_WRITABLE);
    return vaddr;
}

bool x86_mm_free_page(void *vptr, size_t n_page)
{
    size_t page_index = (uintptr_t) vptr / X86_PAGE_SIZE;
    mos_debug("paging: freeing %zu to %zu", page_index, page_index + n_page);
    vm_unmap_page_range((uintptr_t) vptr, n_page);
    return true;
}

void pmem_freelist_setup()
{
    size_t pmem_freelist_size = PMEM_FREELIST_SIZE_FOR(x86_mem_size_available);
    pmem_freelist_size = X86_ALIGN_UP_TO_PAGE(pmem_freelist_size);
    pr_info("paging: %zu bytes (aligned) required for physical memory freelist", pmem_freelist_size);

    uintptr_t paddr = 0;

    // find a free physical memory region that is large enough to map the linked list
    for (int i = x86_mem_regions_count - 1; i >= 0; i++)
    {
        if (!x86_mem_regions[i].available)
            continue;

        if (x86_mem_regions[i].size_bytes < pmem_freelist_size)
            continue;

        // the location should not be lower than the kernel end
        paddr = MAX(x86_mem_regions[i].paddr, x86_kernel_end);
        paddr = X86_ALIGN_UP_TO_PAGE(paddr);

        // compare end address of freelist and the end address of the region
        if (paddr + pmem_freelist_size > x86_mem_regions[i].paddr + x86_mem_regions[i].size_bytes)
        {
            mos_warn("weird: found a free physical memory region, but realised that it's too small after alignment");
            paddr = 0;
            continue;
        }

        pr_info("paging: found free physical memory region at 0x%zx", paddr);
        break;
    }

    pmem_freelist_head = (pmem_range_t *) paddr;
    pmem_freelist_base_paddr = paddr;
    MOS_ASSERT_X(pmem_freelist_head != NULL, "could not find a continous physical memory region, large enough to place pmem freelist");

    // add current physical memory region to the freelist
    for (size_t i = 0; i < x86_mem_regions_count; i++)
    {
        if (!x86_mem_regions[i].available)
            continue;

        size_t alignment_loss = pmem_freelist_add_region(&x86_mem_regions[i]);
        if (alignment_loss)
            pr_emph("paging: %zu bytes of memory loss due to alignment", alignment_loss);
    }

    // map the freelist
    uintptr_t vaddr = PMEM_FREELIST_VADDR;
    vm_map_page_range(vaddr, paddr, pmem_freelist_size / X86_PAGE_SIZE, PAGING_PRESENT | PAGING_WRITABLE);
    pr_info("paging: mapped freelist at virtual address 0x%zx", vaddr);
    pr_info("paging: pmem freelist has %zu entries", pmem_freelist_count);
}

size_t pmem_freelist_add_region(memblock_t *range)
{
    const uintptr_t start_addr = (uintptr_t) range->paddr;
    const uintptr_t end_addr = start_addr + range->size_bytes;

    const uintptr_t aligned_start = X86_ALIGN_UP_TO_PAGE(start_addr);
    const uintptr_t aligned_end = X86_ALIGN_DOWN_TO_PAGE(end_addr);

    const size_t pages_in_region = (aligned_end - aligned_start) / X86_PAGE_SIZE;

    pr_info2("paging: adding physical memory region 0x%.8zx-0x%.8zx to freelist.", aligned_start, aligned_end);

    pmem_range_t *this = pmem_freelist_head;
    pmem_range_t *prev = NULL;

    while (this->next)
    {
        pmem_range_t *next = this->next;
        {
            if (next->paddr > aligned_start)
            {
                // insert before next
                pmem_range_t *new = &pmem_freelist_head[++pmem_freelist_count];
                new->paddr = aligned_start;
                new->n_pages = pages_in_region;
                new->next = this;
                if (prev)
                    prev->next = new;
                else
                    pmem_freelist_head = new;
                goto end;
            }
        }
        prev = this;
        this = next;
    }

    // the freelist is in the correct order, so we can insert the new range here
    this->paddr = aligned_start;
    this->n_pages = pages_in_region;

    pmem_range_t *new = &pmem_freelist_head[++pmem_freelist_count];
    memset(new, 0, sizeof(pmem_range_t));
    new->next = NULL;
    this->next = new;

end:
    return (aligned_start - start_addr) + (end_addr - aligned_end);
}

size_t pmem_freelist_remove_region(memblock_t *range)
{
    const uintptr_t start_addr = (uintptr_t) range->paddr;
    const uintptr_t end_addr = start_addr + range->size_bytes;

    const uintptr_t aligned_start = X86_ALIGN_UP_TO_PAGE(start_addr);
    const uintptr_t aligned_end = X86_ALIGN_DOWN_TO_PAGE(end_addr);

    // const size_t pages_in_region = (aligned_end - aligned_start) / X86_PAGE_SIZE;

    pr_info2("paging: removing physical memory region 0x%.8zx-0x%.8zx from freelist.", aligned_start, aligned_end);
    MOS_UNIMPLEMENTED("paging: remove physical memory region from freelist");

    // pmem_range_t *this = pmem_freelist_head;
    // pmem_range_t *prev = NULL;
    // while (this->next)
    // {
    //     pmem_range_t *next = this->next;
    //     {
    //         if (next->paddr == aligned_start)
    //         {
    //             // remove next
    //             if (prev)
    //                 prev->next = next->next;
    //             else
    //                 pmem_freelist_head = next->next;
    //             pmem_freelist_count--;
    //             goto end;
    //         }
    //     }
    //     prev = this;
    //     this = next;
    // }

    // // the freelist is in the correct order, so we can insert the new range here
    // mos_warn("paging: could not find physical memory region 0x%.8zx-0x%.8zx in freelist.", aligned_start, aligned_end);

    return 0;
}

void vm_map_page_range(uintptr_t vaddr_start, uintptr_t paddr_start, size_t n_page, uint32_t flags)
{
    for (size_t i = 0; i < n_page; i++)
        _impl_vm_map_page(vaddr_start + i * X86_PAGE_SIZE, paddr_start + i * X86_PAGE_SIZE, flags);
}

void vm_unmap_page_range(uintptr_t vaddr_start, size_t n_page)
{
    for (size_t i = 0; i < n_page; i++)
        _impl_vm_unmap_page(vaddr_start + i * X86_PAGE_SIZE);
}

void _impl_vm_map_page(uintptr_t vaddr, uintptr_t paddr, paging_entry_flags flags)
{
    // ensure the page is aligned to 4096
    MOS_ASSERT_X(paddr < X86_MAX_MEM_SIZE, "physical address out of bounds");
    MOS_ASSERT_X(flags < 0x100, "invalid flags");
    MOS_ASSERT_X(vaddr % X86_PAGE_SIZE == 0, "vaddr is not aligned to 4096");

    // ! todo: ensure the offsets are correct for both paddr and vaddr
    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff; // mask out the lower 12 bits

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

    MOS_ASSERT_X(page_table->present == false, "page is already mapped");

    page_table->present = !!(flags & PAGING_PRESENT);
    page_table->writable = !!(flags & PAGING_WRITABLE);
    page_table->usermode = !!(flags & PAGING_USERMODE);
    page_table->phys_addr = (uintptr_t) paddr >> 12;

    // update the mm_page_map
    u32 pte_index = page_dir_index * 1024 + page_table_index;
    PAGEMAP_MAP(pte_index);
}

void _impl_vm_unmap_page(uintptr_t vaddr)
{
    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff;

    pgdir_entry *page_dir = mm_page_dir + page_dir_index;
    if (unlikely(!page_dir->present))
    {
        mos_panic("page '%zx' not mapped", vaddr);
        return;
    }

    pgtable_entry *page_table = ((pgtable_entry *) (page_dir->page_table_addr << 12)) + page_table_index;
    page_table->present = false;

    // update the mm_page_map
    u32 pte_index = page_dir_index * 1024 + page_table_index;
    PAGEMAP_UNMAP(pte_index);
}
