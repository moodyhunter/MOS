// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/paging.h"

#include "lib/stdlib.h"
#include "lib/string.h"
#include "mos/kconfig.h"
#include "mos/mm/mm_types.h"
#include "mos/platform/platform.h"
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
    uintptr_t paddr;
    size_t n_pages;
} pmem_range_t;

#define PMEM_FREELIST_SIZE_FOR(mem_size) ((mem_size / 2 / X86_PAGE_SIZE) * sizeof(pmem_range_t))

#define PMEM_FREELIST_VADDR           0xFFA00000
#define PMEM_FREELIST_LOWMEM_RESERVED 1 MB

// The list holding free memory ranges, this list terminates with [an entry with next == NULL]
static u32 pmem_freelist_base_paddr;
static pmem_range_t *pmem_freelist = NULL;
static size_t pmem_freelist_count = 0;

static_assert((u64) PMEM_FREELIST_VADDR + PMEM_FREELIST_SIZE_FOR(X86_MAX_MEM_SIZE) < (u64) X86_MAX_MEM_SIZE, "no enough vspace for freelist!");

void x86_mm_prepare_paging()
{
    // validate if the memory region calculated from the linker script is correct.
    s64 paging_area_size = (uintptr_t) x86_paging_area_end - (uintptr_t) x86_paging_area_start;
    static const s64 paging_area_size_expected = 1024 * sizeof(pgdir_entry) + 1024 * 1024 * sizeof(pgtable_entry);
    mos_debug("paging: provided size: 0x%llu, minimum required size: 0x%llu", paging_area_size, paging_area_size_expected);
    MOS_ASSERT_X(paging_area_size >= paging_area_size_expected, "allocated paging area size is too small");

    // place the global page directory at somewhere outside of the kernel
    mm_page_dir = (pgdir_entry *) x86_paging_area_start;
    mm_page_table = (pgtable_entry *) x86_page_table_start;

    MOS_ASSERT_X((uintptr_t) mm_page_dir + 1024 * sizeof(pgtable_entry) <= (uintptr_t) mm_page_table, "overlap between page table & directory");
    MOS_ASSERT_X((uintptr_t) mm_page_dir % X86_PAGE_SIZE == 0, "page directory is not aligned to 4096");
    MOS_ASSERT_X((uintptr_t) mm_page_table % X86_PAGE_SIZE == 0, "page table is not aligned to 4096");

    // initialize the page directory
    memset(mm_page_dir, 0, sizeof(pgdir_entry) * 1024);
    memset(mm_page_table, 0, sizeof(pgtable_entry) * 1024 * 1024);

    pr_info("paging: setting up physical memory freelist...");
    pmem_freelist_setup();

    pr_info("paging: setting up low 1MB identity mapping... (except for the NULL page)");
    // skip the free list setup
    vm_map_page_range_no_freelist(0, 0, 1, VM_PRESENT); // ! the zero page is not writable
    vm_map_page_range_no_freelist(X86_PAGE_SIZE, X86_PAGE_SIZE, 1 MB / X86_PAGE_SIZE, VM_PRESENT | VM_WRITABLE);

    pr_info("paging: setting kernel space...");
    vm_map_page_range(x86_kernel_start, x86_kernel_start, (x86_kernel_end - x86_kernel_start) / X86_PAGE_SIZE, VM_PRESENT | VM_WRITABLE);
}

void x86_mm_enable_paging()
{
    pr_info("paging: converting physical memory freelist to vm mode");
    pmem_range_t *this = pmem_freelist;
    while (this->next)
    {
        pmem_range_t *pnext = this->next;
        this->next = (pmem_range_t *) (PMEM_FREELIST_VADDR + (uintptr_t) pnext - pmem_freelist_base_paddr);
        this = pnext;
    }
    pmem_freelist = (pmem_range_t *) (PMEM_FREELIST_VADDR + (uintptr_t) pmem_freelist - pmem_freelist_base_paddr);
    pmem_freelist_base_paddr = PMEM_FREELIST_VADDR;

    pr_info("paging: mapped freelist at virtual address " PTR_FMT, (uintptr_t) pmem_freelist);
    pr_info("paging: page directory at: %p", (void *) mm_page_dir);
    x86_enable_paging_impl(mm_page_dir);
    pr_info("paging: Enabled");
    pmem_freelist_dump();
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

    size_t page_i = target_pagemap_start * PAGEMAP_WIDTH + target_bit;
    void *vaddr = (void *) (page_i * X86_PAGE_SIZE);
    mos_debug("paging: allocating page %zu to %zu (aka starting at %p)", page_i, page_i + n_page, vaddr);

    uintptr_t paddr = pmem_freelist_get_page(n_page);

    if (paddr == 0)
    {
        mos_panic("OOM");
        return NULL;
    }

    vm_map_page_range((uintptr_t) vaddr, paddr, n_page, VM_PRESENT | VM_WRITABLE);
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
    pr_info2("paging: %zu bytes (aligned) required for physical memory freelist", pmem_freelist_size);

    uintptr_t paddr = 0;

    // find a free physical memory region that is large enough to map the linked list
    for (int i = x86_mem_regions_count - 1; i >= 0; i--)
    {
        memblock_t *region = &x86_mem_regions[i];
        if (!region->available)
            continue;

        if (region->size_bytes < pmem_freelist_size)
            continue;

        // the location should not be lower than the kernel end
        paddr = MAX(region->paddr, x86_kernel_end);
        paddr = X86_ALIGN_UP_TO_PAGE(paddr);

        // compare end address of freelist and the end address of the region
        if (paddr + pmem_freelist_size > region->paddr + region->size_bytes)
        {
            mos_warn("weird: found a free physical memory region, but realised that it's too small after alignment");
            paddr = 0;
            continue;
        }

        pr_info2("paging: found free physical memory region for freelist at " PTR_FMT, paddr);
        break;
    }

    pmem_freelist = (pmem_range_t *) paddr;
    memset(pmem_freelist, 0, pmem_freelist_size);
    pmem_freelist_base_paddr = paddr;
    MOS_ASSERT_X(pmem_freelist != NULL, "could not find a continous physical memory region, large enough to place pmem freelist");
    pmem_freelist->next = NULL;

    // add current physical memory region to the freelist
    for (size_t i = 0; i < x86_mem_regions_count; i++)
    {
        memblock_t *r = &x86_mem_regions[i];
        if (!r->available)
            continue;

        if (r->paddr + r->size_bytes < PMEM_FREELIST_LOWMEM_RESERVED)
        {
            pr_emph("paging: ignored low memory: 0x%llx - 0x%llx (0x%zu bytes)", r->paddr, r->paddr + r->size_bytes, r->size_bytes);
            continue;
        }

        size_t alignment_loss = pmem_freelist_add_region(r->paddr, r->size_bytes);
        if (alignment_loss)
            pr_emph("paging: %zu bytes of memory loss due to alignment", alignment_loss);
    }

    // map the freelist
    uintptr_t vaddr = PMEM_FREELIST_VADDR;
    pr_info("paging: mapping freelist at physical address %p", (void *) paddr);
    vm_map_page_range(vaddr, paddr, pmem_freelist_size / X86_PAGE_SIZE, VM_PRESENT | VM_WRITABLE);
}

#define this_start (this->paddr)
#define this_end   (this_start + this->n_pages * X86_PAGE_SIZE)

size_t pmem_freelist_add_region(uintptr_t start_addr, size_t size_bytes)
{
    const uintptr_t end_addr = start_addr + size_bytes;

    const uintptr_t aligned_start = X86_ALIGN_UP_TO_PAGE(start_addr);
    const uintptr_t aligned_end = X86_ALIGN_DOWN_TO_PAGE(end_addr);

    const size_t pages_in_region = (aligned_end - aligned_start) / X86_PAGE_SIZE;

    mos_debug("paging: adding physical memory region " PTR_FMT "-" PTR_FMT " to freelist.", aligned_start, aligned_end);

    pmem_range_t *this = pmem_freelist;
    pmem_range_t *prev = NULL;

    // if "->next" is NULL, then we are at the end of the list
    while (this->next)
    {
        // the new region should not overlap with the current region
        if ((this_start <= aligned_start && aligned_start < this_end) || (this_start < aligned_end && aligned_end <= this_end))
        {
            mos_panic("added pmem " PTR_FMT "-" PTR_FMT " overlaps with region " PTR_FMT "-" PTR_FMT, aligned_start, aligned_end, this_start,
                      this_end);
            return 0;
        }

        // prepend to 'this' region
        if (this_start == aligned_end)
        {
            this->paddr = aligned_start;
            this->n_pages += pages_in_region;
            mos_debug("paging: enlarged " PTR_FMT "-" PTR_FMT ": starts at " PTR_FMT, this_start, this_end,
                      this_start + pages_in_region * X86_PAGE_SIZE);
            goto end;
        }

        // append to the previous region
        // * the new region "may" overlap with next region
        // * if we 'append' to the current region, we will not be able to check for such overlap
        // * (because we would have 'goto end'-ed)
        const uintptr_t prev_start = prev ? prev->paddr : 0;
        const uintptr_t prev_end = prev ? prev_start + prev->n_pages * X86_PAGE_SIZE : 0;
        if (prev && prev_end == aligned_start)
        {
            prev->n_pages += pages_in_region;
            mos_debug("paging: enlarged " PTR_FMT "-" PTR_FMT ": ends at " PTR_FMT, prev_start, prev_end,
                      prev_end + pages_in_region * X86_PAGE_SIZE);
            goto end;
        }

        // same as above, we only 'prepend' the region before 'this'
        if (aligned_end < this_start)
        {
            pmem_range_t *new = (pmem_range_t *) pmem_freelist_base_paddr + pmem_freelist_count++;
            new->paddr = aligned_start;
            new->n_pages = pages_in_region;
            new->next = this;
            if (prev)
                prev->next = new;
            else
                pmem_freelist = new;
            goto end;
        }

        prev = this;
        this = this->next;
    }

    MOS_ASSERT(this->next == NULL);
    pmem_range_t *new = (pmem_range_t *) pmem_freelist_base_paddr + pmem_freelist_count++;
    this->next = new;

    // the freelist is in the correct order, so we can insert the new range here
    new->paddr = aligned_start;
    new->n_pages = pages_in_region;
    new->next = NULL;

end:
    return (aligned_start - start_addr) + (end_addr - aligned_end);
}

void pmem_freelist_remove_region(uintptr_t start_addr, size_t size_bytes)
{
    const uintptr_t end_addr = start_addr + size_bytes;
    const size_t pages_in_region = (end_addr - start_addr) / X86_PAGE_SIZE;

    // the addresses must be aligned to page size
    MOS_ASSERT(X86_ALIGN_UP_TO_PAGE(start_addr) == start_addr);
    MOS_ASSERT(X86_ALIGN_DOWN_TO_PAGE(end_addr) == end_addr);

    mos_debug("paging: removing physical memory region " PTR_FMT "-" PTR_FMT " from freelist.", start_addr, end_addr);

    bool needs_cleanup = false;
    bool freed = false;
    void *cleanup_target_memptr = NULL;

    pmem_range_t *this = pmem_freelist;
    pmem_range_t *prev = NULL;
    while (this)
    {
        pmem_range_t *next = this->next;
        if (this_start <= start_addr && this_end >= end_addr)
        {
            // starts before this region
            if (start_addr < this_start)
                continue;

            // or ends after this region
            if (end_addr > this_end)
                continue;

            const size_t part_1_size = start_addr - this_start;
            const size_t part_2_size = this_end - end_addr;

            if (part_1_size == 0 && part_2_size != 0)
            {
                // part 1 is empty, which means we are removing from the front of the region
                this->paddr = end_addr;
                this->n_pages = part_2_size / X86_PAGE_SIZE;
                mos_debug("paging: this pmem block now starts at " PTR_FMT ", with %zu pages", this_start, this->n_pages);
            }
            else if (part_1_size != 0 && part_2_size == 0)
            {
                // part 2 is empty, which means we are removing the tail of part 1
                mos_debug("paging: shrunk " PTR_FMT "-" PTR_FMT ": ends at " PTR_FMT, this_start, this_end,
                          this_start + (this->n_pages - pages_in_region) * X86_PAGE_SIZE);
                this->n_pages -= pages_in_region;
            }
            else if (part_1_size == 0 && part_2_size == 0)
            {
                // part 1 and part 2 are empty, so remove this entry
                if (prev)
                    prev->next = next;
                else
                    pmem_freelist = next;

                // have to do some cleanup
                needs_cleanup = true;
                cleanup_target_memptr = this;
                mos_debug("paging: removed " PTR_FMT "-" PTR_FMT " from freelist.", this_start, this_end);
            }
            else
            {
                // neither part 1 nor part 2 is empty, so we have to allocate a new entry for part 2
                mos_debug("paging: split " PTR_FMT "-" PTR_FMT " into " PTR_FMT "-" PTR_FMT " and " PTR_FMT "-" PTR_FMT, this_start, this_end,
                          this_start, start_addr, end_addr, end_addr + part_2_size);
                pmem_range_t *new = (pmem_range_t *) pmem_freelist_base_paddr + pmem_freelist_count++;
                memset(new, 0, sizeof(pmem_range_t));
                new->next = next;
                this->next = new;
                this->n_pages = part_1_size / X86_PAGE_SIZE;
                new->paddr = end_addr;
                new->n_pages = part_2_size / X86_PAGE_SIZE;
            }
            freed = true;
            break;
        }
        prev = this;
        this = next;
    }

    if (!freed)
        mos_panic("paging: " PTR_FMT "-" PTR_FMT " is not in the freelist.", start_addr, end_addr);

    if (needs_cleanup)
    {
        // firstly find who is at 'pmem_freelist_count' that should be copied to 'cleanup_target_memptr'
        // minus 1 because it's a count, make it an index
        pmem_range_t *copy_source = (pmem_range_t *) pmem_freelist_base_paddr + pmem_freelist_count - 1;
        pmem_freelist_count--;

        pmem_range_t *r_prev = NULL;
        for (pmem_range_t *r_this = pmem_freelist; r_this != copy_source && r_this->next; r_this = r_this->next)
            r_prev = r_this;

        if (cleanup_target_memptr != copy_source)
        {
            memmove(cleanup_target_memptr, copy_source, sizeof(pmem_range_t));
            memset(copy_source, 0, sizeof(pmem_range_t)); // prevent from being used again

            if (copy_source != r_prev->next)
            {
                pmem_freelist = cleanup_target_memptr;
            }
            else
            {
                r_prev->next = cleanup_target_memptr;
            }
        }
    }
}
#undef this_start
#undef this_end

uintptr_t pmem_freelist_get_page(size_t pages)
{
    pmem_range_t *this = pmem_freelist;
    while (this)
    {
        if (this->n_pages >= pages)
        {
            // ! do not remove the range from the freelist
            uintptr_t addr = this->paddr;
            mos_debug("paging: allocated %zu pages from freelist, starting at " PTR_FMT, pages, addr);
            return addr;
        }
        this = this->next;
    }

    // !! OOM
    mos_warn("paging: out of memory?");
    return 0;
}

void pmem_freelist_dump()
{
    pr_info("paging: pmem freelist has %zu entries", pmem_freelist_count);
    pmem_range_t *this = pmem_freelist;
    while (this)
    {
        const uintptr_t pstart = this->paddr;
        const uintptr_t pend = pstart + this->n_pages * X86_PAGE_SIZE;
        char sbuf[32];
        format_size(sbuf, sizeof(sbuf), this->n_pages * X86_PAGE_SIZE);
        pr_info("  [%p] entry: " PTR_FMT "-" PTR_FMT " (%zu page(s), %s)", (void *) this, pstart, pend, this->n_pages, sbuf);
        this = this->next;
    }
}

void page_table_dump()
{
    static const s32 NONE = -1;
    pr_info("paging: dumping page table");

    s32 present_begin = NONE;
    s32 absent_begin = NONE;

    for (int pgd_i = 0; pgd_i < 1024; pgd_i++)
    {
        pgdir_entry *pgd = mm_page_dir + pgd_i;
        if (!pgd->present)
        {
            if (absent_begin == NONE)
                absent_begin = pgd_i * 1024;
            continue;
        }

        for (int pgt_i = 0; pgt_i < 1024; pgt_i++)
        {
            uintptr_t table_id = pgd_i * 1024 + pgt_i;
            pgtable_entry *pgt = mm_page_table + table_id;
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

void vm_map_page_range(uintptr_t vaddr_start, uintptr_t paddr_start, size_t n_page, u32 flags)
{
    pmem_freelist_remove_region(paddr_start, n_page * X86_PAGE_SIZE);
    vm_map_page_range_no_freelist(vaddr_start, paddr_start, n_page, flags);
}

void vm_unmap_page_range(uintptr_t vaddr_start, size_t n_page)
{
    uintptr_t paddr = vm_get_paddr(vaddr_start);
    vm_unmap_page_range_no_freelist(vaddr_start, n_page);
    pmem_freelist_add_region(paddr, n_page * X86_PAGE_SIZE);
}

void vm_map_page_range_no_freelist(uintptr_t vaddr_start, uintptr_t paddr_start, size_t n_page, u32 flags)
{
    mos_debug("paging: mapping %u pages " PTR_FMT " to " PTR_FMT " at table %lu", n_page, vaddr_start, paddr_start, vaddr_start / X86_PAGE_SIZE);
    for (size_t i = 0; i < n_page; i++)
        _impl_vm_map_page(vaddr_start + i * X86_PAGE_SIZE, paddr_start + i * X86_PAGE_SIZE, flags);
}

void vm_unmap_page_range_no_freelist(uintptr_t vaddr_start, size_t n_page)
{
    mos_debug("paging: unmapping %u pages " PTR_FMT " at table %lu", n_page, vaddr_start, vaddr_start / X86_PAGE_SIZE);
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

    page_dir->writable |= !!(flags & VM_WRITABLE);
    page_dir->usermode |= !!(flags & VM_USERMODE);

    MOS_ASSERT_X(page_table->present == false, "page is already mapped");

    page_table->present = !!(flags & VM_PRESENT);
    page_table->writable = !!(flags & VM_WRITABLE);
    page_table->usermode = !!(flags & VM_USERMODE);
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
        mos_panic("vmem '%lx' not mapped", vaddr);
        return;
    }

    pgtable_entry *page_table = ((pgtable_entry *) (page_dir->page_table_addr << 12)) + page_table_index;
    page_table->present = false;

    // update the mm_page_map
    u32 pte_index = page_dir_index * 1024 + page_table_index;
    PAGEMAP_UNMAP(pte_index);
}

uintptr_t vm_get_paddr(uintptr_t vaddr)
{
    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff;
    pgdir_entry *page_dir = mm_page_dir + page_dir_index;
    if (unlikely(!page_dir->present))
    {
        mos_panic("page directory for address '%lx' not mapped", vaddr);
        return 0;
    }
    pgtable_entry *page_table = mm_page_table + page_dir_index * 1024 + page_table_index;
    if (unlikely(!page_table->present))
    {
        mos_panic("vmem '%lx' not mapped", vaddr);
        return 0;
    }
    return (page_table->phys_addr << 12) + (vaddr & 0xfff);
}
