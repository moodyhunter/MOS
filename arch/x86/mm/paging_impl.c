// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/mm/paging_impl.h"

#include "mos/constants.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/types.h"
#include "mos/x86/mm/paging.h"
#include "mos/x86/mm/pmem_freelist.h"
#include "mos/x86/x86_platform.h"

#define PAGEMAP_MAP(map, index)   map[index / PAGEMAP_WIDTH] |= 1 << (index % PAGEMAP_WIDTH)
#define PAGEMAP_UNMAP(map, index) map[index / PAGEMAP_WIDTH] &= ~(1 << (index % PAGEMAP_WIDTH))
#define BIT_IS_SET(byte, bit)     ((byte) & (1 << (bit)))

always_inline void pg_flush_tlb(uintptr_t vaddr)
{
    __asm__ volatile("invlpg (%0)" ::"r"(vaddr));
}

vmblock_t pg_page_alloc(x86_pg_infra_t *pg, size_t n_page, pgalloc_hints hints, vm_flags vm_flag)
{
    vmblock_t block = pg_page_get_free(pg, n_page, hints);
    return pg_page_alloc_at(pg, block.vaddr, n_page, vm_flag);
}

vmblock_t pg_page_alloc_at(x86_pg_infra_t *pg, uintptr_t vaddr, size_t n_page, vm_flags vm_flag)
{
    uintptr_t paddr = pmem_freelist_find_free(n_page);

    if (paddr == 0)
    {
        mos_panic("OOM");
        return (vmblock_t){ 0, .npages = 0 };
    }

    pg_map_pages(pg, vaddr, paddr, n_page, vm_flag);
    vmblock_t block = {
        .vaddr = vaddr,
        .npages = n_page,
        .flags = vm_flag,
    };
    return block;
}

vmblock_t pg_page_get_free(x86_pg_infra_t *pg, size_t n_pages, pgalloc_hints flags)
{
    uintptr_t vaddr_begin;

    switch (flags)
    {
        case PGALLOC_HINT_KHEAP: vaddr_begin = MOS_ADDR_KERNEL_HEAP; break;
        case PGALLOC_HINT_UHEAP: vaddr_begin = MOS_ADDR_USER_HEAP; break;
        case PGALLOC_HINT_STACK: vaddr_begin = MOS_ADDR_USER_STACK; break;
        case PGALLOC_HINT_MMAP: vaddr_begin = MOS_ADDR_USER_MMAP; break;
        default: mos_panic("invalid pgalloc_hints"); break;
    }

    // simply rename the variable, we are dealing with bitmaps
    size_t n_bits = n_pages;
    size_t n_zero_bits = 0;

    u8 target_bit = 0;
    uintptr_t vaddr_map_bit_begin = vaddr_begin / MOS_PAGE_SIZE / PAGEMAP_WIDTH + 1;
    for (size_t i = vaddr_map_bit_begin; n_zero_bits < n_bits; i++)
    {
        if (i >= MM_PAGE_MAP_SIZE)
        {
            mos_warn("failed to allocate %zu pages", n_pages);
            return (vmblock_t){ 0, .npages = 0 };
        }
        pagemap_line_t current_byte = pg->page_map[i];

        if (current_byte == 0)
        {
            n_zero_bits += PAGEMAP_WIDTH;
            continue;
        }
        if (current_byte == (pagemap_line_t) ~0)
        {
            vaddr_map_bit_begin = i + 1;
            continue;
        }

        for (size_t bit = 0; bit < PAGEMAP_WIDTH; bit++)
        {
            if (!BIT_IS_SET(current_byte, bit))
                n_zero_bits++;
            else
                n_zero_bits = 0, target_bit = bit + 1, vaddr_map_bit_begin = i;
        }
    }

    size_t page_i = vaddr_map_bit_begin * PAGEMAP_WIDTH + target_bit;
    uintptr_t vaddr = page_i * MOS_PAGE_SIZE;

    return (vmblock_t){ .vaddr = vaddr, .npages = n_pages };
}

void pg_page_free(x86_pg_infra_t *pg, uintptr_t vptr, size_t n_page)
{
    size_t page_index = vptr / MOS_PAGE_SIZE;
    mos_debug("paging: freeing %zu to %zu", page_index, page_index + n_page);
    pg_unmap_pages(pg, vptr, n_page);
}

void pg_page_flag(x86_pg_infra_t *pg, uintptr_t vaddr, size_t n, vm_flags flags)
{
    mos_debug("paging: setting flags [0x%x] to [" PTR_FMT "] +%zu pages", flags, vaddr, n);
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
        pg_flush_tlb(vaddr);
    }
}

void pg_map_pages(x86_pg_infra_t *pg, uintptr_t vaddr_start, uintptr_t paddr_start, size_t n_page, vm_flags flags)
{
    pmem_freelist_remove_region(paddr_start, n_page * MOS_PAGE_SIZE);
    pg_do_map_pages(pg, vaddr_start, paddr_start, n_page, flags);
}

void pg_unmap_pages(x86_pg_infra_t *pg, uintptr_t vaddr_start, size_t n_page)
{
    uintptr_t paddr = pg_page_get_mapped_paddr(pg, vaddr_start);
    pg_do_unmap_pages(pg, vaddr_start, n_page);
    pmem_freelist_add_region(paddr, n_page * MOS_PAGE_SIZE);
}

void pg_do_map_pages(x86_pg_infra_t *pg, uintptr_t vaddr_start, uintptr_t paddr_start, size_t n_page, vm_flags flags)
{
    mos_debug("paging: mapping %zu pages (" PTR_FMT "->" PTR_FMT ") @ table %lu", n_page, vaddr_start, paddr_start, vaddr_start / MOS_PAGE_SIZE);
    for (size_t i = 0; i < n_page; i++)
        pg_do_map_page(pg, vaddr_start + i * MOS_PAGE_SIZE, paddr_start + i * MOS_PAGE_SIZE, flags);
}

void pg_do_unmap_pages(x86_pg_infra_t *pg, uintptr_t vaddr_start, size_t n_page)
{
    mos_debug("paging: unmapping %zu pages starting at " PTR_FMT " @ table %lu", n_page, vaddr_start, vaddr_start / MOS_PAGE_SIZE);
    for (size_t i = 0; i < n_page; i++)
        pg_do_unmap_page(pg, vaddr_start + i * MOS_PAGE_SIZE);
}

void pg_do_map_page(x86_pg_infra_t *pg, uintptr_t vaddr, uintptr_t paddr, vm_flags flags)
{
    // ensure the page is aligned to 4096
    MOS_ASSERT_X(paddr < X86_MAX_MEM_SIZE, "physical address out of bounds");
    MOS_ASSERT_X(vaddr % MOS_PAGE_SIZE == 0, "vaddr is not aligned to 4096");

    // ! todo: ensure the offsets are correct for both paddr and vaddr
    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff; // mask out the lower 12 bits

    u32 pte_index = page_dir_index * 1024 + page_table_index;

    x86_pgdir_entry *this_dir = &pg->pgdir[page_dir_index];

    x86_pgtable_entry *this_table = &pg->pgtable[pte_index];
    if (unlikely(!this_dir->present))
    {
        this_dir->present = true;

        // kernel page tables are identity mapped
        uintptr_t table_paddr;
        if (pg == x86_kpg_infra)
            table_paddr = (uintptr_t) this_table - MOS_KERNEL_START_VADDR;
        else
            table_paddr = pg_page_get_mapped_paddr(x86_kpg_infra, (uintptr_t) this_table);
        this_dir->page_table_paddr = table_paddr >> 12;
    }

    if (pg != x86_kpg_infra && page_dir_index >= 768)
        MOS_ASSERT_X(this_dir->present, "page directory not present for kernel addresses, when mapping user pages");

    MOS_ASSERT_X(this_table->present == false, "page is already mapped");

    this_table->present = true;
    this_table->phys_addr = (uintptr_t) paddr >> 12;

    this_dir->writable |= flags & VM_WRITE;
    this_table->writable = flags & VM_WRITE;

    this_dir->usermode |= flags & VM_USER;
    this_table->usermode = flags & VM_USER;

    this_dir->cache_disabled |= flags & VM_CACHE_DISABLED;
    this_table->cache_disabled = flags & VM_CACHE_DISABLED;

    this_table->global = flags & VM_GLOBAL;

    // update the mm_page_map
    PAGEMAP_MAP(pg->page_map, pte_index);
    pg_flush_tlb(vaddr);
}

void pg_do_unmap_page(x86_pg_infra_t *pg, uintptr_t vaddr)
{
    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff;

    x86_pgdir_entry *page_dir = &pg->pgdir[page_dir_index];
    if (unlikely(!page_dir->present))
    {
        mos_panic("vmem " PTR_FMT " not mapped", vaddr);
        return;
    }

    x86_pgtable_entry *page_table = &pg->pgtable[page_dir_index * 1024 + page_table_index];
    page_table->present = false;

    // update the mm page map
    u32 pte_index = page_dir_index * 1024 + page_table_index;
    PAGEMAP_UNMAP(pg->page_map, pte_index);
    pg_flush_tlb(vaddr);
}

// !! TODO: read real address instead of assuming memory layout = x86_pg_infra_t
uintptr_t pg_page_get_mapped_paddr(x86_pg_infra_t *pg, uintptr_t vaddr)
{
    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff;
    x86_pgdir_entry *page_dir = pg->pgdir + page_dir_index;
    x86_pgtable_entry *page_table = pg->pgtable + page_dir_index * 1024 + page_table_index;

    if (page_dir_index >= 768)
        page_dir = x86_kpg_infra->pgdir + page_dir_index, page_table = x86_kpg_infra->pgtable + page_dir_index * 1024 + page_table_index;

    if (unlikely(!page_dir->present))
        mos_panic("page directory for address " PTR_FMT " not mapped", vaddr);

    if (unlikely(!page_table->present))
        mos_panic("vmem " PTR_FMT " not mapped", vaddr);

    return (page_table->phys_addr << 12) + (vaddr & 0xfff);
}

vm_flags pg_page_get_flags(x86_pg_infra_t *pg, uintptr_t vaddr)
{
    int page_dir_index = vaddr >> 22;
    int page_table_index = vaddr >> 12 & 0x3ff;
    x86_pgdir_entry *page_dir = pg->pgdir + page_dir_index;
    x86_pgtable_entry *page_table = pg->pgtable + page_dir_index * 1024 + page_table_index;

    if (page_dir_index >= 768)
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
