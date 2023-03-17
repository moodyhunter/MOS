// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/paging.h"

#include "lib/structures/bitmap.h"
#include "lib/sync/spinlock.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging/pmalloc.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

#define PGD_FOR_VADDR(_vaddr, _um) (_vaddr >= MOS_KERNEL_START_VADDR ? platform_info->kernel_pgd : _um)

static bitmap_line_t kernel_page_map[MOS_PAGEMAP_KERNEL_LINES] = { 0 };
static spinlock_t kernel_page_map_lock = SPINLOCK_INIT;

// ! BEGIN: PAGEMAP
static void pagemap_mark_used(page_map_t *map, uintptr_t vaddr, size_t n_pages)
{
    MOS_ASSERT_X(vaddr % MOS_PAGE_SIZE == 0, "vaddr is not page aligned");
    if (n_pages == 0)
    {
        mos_warn("pagemap_mark_used: n_pages is 0");
        return;
    }

    const bool is_kernel = vaddr >= MOS_KERNEL_START_VADDR;
    const uintptr_t pagemap_base = is_kernel ? MOS_KERNEL_START_VADDR : 0;
    const size_t pagemap_size_lines = is_kernel ? MOS_PAGEMAP_KERNEL_LINES : MOS_PAGEMAP_USER_LINES;
    const size_t pagemap_index = (vaddr - pagemap_base) / MOS_PAGE_SIZE;

    if (!is_kernel)
        MOS_ASSERT_X(map, "map is null for a user pagetable");

    bitmap_line_t *pagemap = is_kernel ? kernel_page_map : map->ummap;
    spinlock_t *lock = is_kernel ? &kernel_page_map_lock : &map->lock;
    MOS_ASSERT_X(pagemap && lock, "pagemap or lock is NULL");

    if (unlikely(pagemap_index + n_pages > pagemap_size_lines * BITMAP_LINE_BITS))
    {
        mos_panic("pagemap_use: out of bounds");
    }

    spinlock_acquire(lock);
    for (size_t i = 0; i < n_pages; i++)
    {
        bool is_set = bitmap_get(pagemap, pagemap_size_lines, pagemap_index + i);
        MOS_ASSERT_X(!is_set, "page " PTR_FMT " is already allocated", pagemap_base + (pagemap_index + i) * MOS_PAGE_SIZE);
        bitmap_set(pagemap, pagemap_size_lines, pagemap_index + i);
        is_set = bitmap_get(pagemap, pagemap_size_lines, pagemap_index + i);
        MOS_ASSERT_X(is_set, "page " PTR_FMT " is not set", pagemap_base + (pagemap_index + i) * MOS_PAGE_SIZE);
    }
    spinlock_release(lock);
}

static void pagemap_mark_free(page_map_t *map, uintptr_t vaddr, size_t n_pages)
{
    const bool is_kernel = vaddr >= MOS_KERNEL_START_VADDR;
    const uintptr_t pagemap_base = is_kernel ? MOS_KERNEL_START_VADDR : 0;
    const size_t pagemap_size_lines = is_kernel ? MOS_PAGEMAP_KERNEL_LINES : MOS_PAGEMAP_USER_LINES;
    const size_t pagemap_index = (vaddr - pagemap_base) / MOS_PAGE_SIZE;

    bitmap_line_t *pagemap = is_kernel ? kernel_page_map : map->ummap;
    spinlock_t *lock = is_kernel ? &kernel_page_map_lock : &map->lock;
    MOS_ASSERT_X(pagemap && lock, "pagemap or lock is NULL");

    if (unlikely(pagemap_index + n_pages > pagemap_size_lines * BITMAP_LINE_BITS))
    {
        mos_panic("pagemap_free: out of bounds");
    }

    spinlock_acquire(lock);
    for (size_t i = 0; i < n_pages; i++)
    {
        bool is_set = bitmap_get(pagemap, pagemap_size_lines, pagemap_index + i);
        MOS_ASSERT_X(is_set, "page " PTR_FMT " is already free", pagemap_base + (uintptr_t) (pagemap_index + i) * MOS_PAGE_SIZE);
        bitmap_clear(pagemap, pagemap_size_lines, pagemap_index + i);
    }
    spinlock_release(lock);
}
// ! END: PAGEMAP

uintptr_t mm_get_free_pages(paging_handle_t table, size_t n_pages, pgalloc_hints hints)
{
    static const uintptr_t limits[] = {
        [PGALLOC_HINT_KHEAP] = MOS_ADDR_KERNEL_HEAP,
        [PGALLOC_HINT_UHEAP] = MOS_ADDR_USER_HEAP,
        [PGALLOC_HINT_STACK] = MOS_ADDR_USER_STACK,
        [PGALLOC_HINT_MMAP] = MOS_ADDR_USER_MMAP,
    };

    const uintptr_t vaddr_begin = limits[hints];

    const bool is_kernel = hints == PGALLOC_HINT_KHEAP;
    bitmap_line_t *pagemap = is_kernel ? kernel_page_map : table.um_page_map->ummap;
    spinlock_t *lock = is_kernel ? &kernel_page_map_lock : &table.um_page_map->lock;
    const uintptr_t pagemap_base = is_kernel ? MOS_KERNEL_START_VADDR : 0;
    const size_t pagemap_size_lines = is_kernel ? MOS_PAGEMAP_KERNEL_LINES : MOS_PAGEMAP_USER_LINES;
    const size_t pagemap_begin_index = (vaddr_begin - pagemap_base) / MOS_PAGE_SIZE;

    spinlock_acquire(lock);
    const size_t page_i = bitmap_find_first_free_n(pagemap, pagemap_size_lines, pagemap_begin_index, n_pages);
    if (unlikely(page_i == 0))
    {
        spinlock_release(lock);
        pr_warn("no contiguous %zu pages found in pagemap", n_pages);
        return 0;
    }

    const bool is_set = bitmap_get(pagemap, pagemap_size_lines, page_i);
    MOS_ASSERT_X(!is_set, "page %zu is already allocated", page_i);
    spinlock_release(lock);

    const uintptr_t vaddr = pagemap_base + page_i * MOS_PAGE_SIZE;
    const bool begins_in_kernel = vaddr >= MOS_KERNEL_START_VADDR;
    const bool ends_in_kernel = (vaddr + n_pages * MOS_PAGE_SIZE) >= MOS_KERNEL_START_VADDR;
    if (is_kernel)
    {
        MOS_ASSERT_X(begins_in_kernel && ends_in_kernel, "incorrect allocation, expected in kernel space");
    }
    else
    {
        MOS_ASSERT_X(!begins_in_kernel && !ends_in_kernel, "incorrect allocation, expected in user space");
    }

    return vaddr;
}

vmblock_t mm_alloc_pages(paging_handle_t table, size_t npages, pgalloc_hints hints, vm_flags flags)
{
    MOS_ASSERT(npages > 0);

    if (unlikely(table.pgd == 0))
    {
        mos_warn("cannot allocate %zd pages, pagetable is null", npages);
        return (vmblock_t){ 0 };
    }

    const uintptr_t vaddr_base = mm_get_free_pages(table, npages, hints);
    if (unlikely(vaddr_base == 0))
    {
        mos_warn("could not find free %zd pages", npages);
        return (vmblock_t){ 0 };
    }
    return mm_alloc_pages_at(table, vaddr_base, npages, flags);
}

vmblock_t mm_alloc_pages_at(paging_handle_t table, uintptr_t vaddr, size_t n_pages, vm_flags flags)
{
    MOS_ASSERT(n_pages > 0);

    if (unlikely(table.pgd == 0))
    {
        mos_warn("cannot allocate %zd pages at " PTR_FMT ", pagetable is null", n_pages, vaddr);
        return (vmblock_t){ .vaddr = 0, .npages = 0 };
    }

    uintptr_t paddr = pmalloc_alloc(n_pages);
    if (unlikely(paddr == 0))
    {
        mos_warn("could not allocate %zd physical pages", n_pages);
        return (vmblock_t){ 0 };
    }

    vmblock_t block = { .vaddr = vaddr, .npages = n_pages, .paddr = paddr, .flags = flags, .address_space = table };
    mm_map_allocated_pages(table, block);
    return block;
}

void mm_free_pages(paging_handle_t table, vmblock_t block)
{
    mm_unmap_pages(table, block.vaddr, block.npages);
    pmalloc_release_pages(block.paddr, block.npages);
}

void mm_map_pages(paging_handle_t table, vmblock_t block)
{
    if (unlikely(table.pgd == 0))
    {
        mos_warn("cannot map pages at " PTR_FMT ", pagetable is null", block.vaddr);
        return;
    }

    pmalloc_acquire_pages(block.paddr, block.npages);
    mm_map_allocated_pages(table, block);
}

void mm_map_allocated_pages(paging_handle_t table, vmblock_t block)
{
    if (unlikely(table.pgd == 0))
    {
        mos_warn("cannot map pages at " PTR_FMT ", pagetable is null", block.vaddr);
        return;
    }

    pagemap_mark_used(table.um_page_map, block.vaddr, block.npages);
    platform_mm_map_pages(PGD_FOR_VADDR(block.vaddr, table), block.vaddr, block.paddr, block.npages, block.flags);
}

void mm_unmap_pages(paging_handle_t table, uintptr_t vaddr, size_t npages)
{
    MOS_ASSERT(npages > 0);

    pagemap_mark_free(table.um_page_map, vaddr, npages);
    platform_mm_unmap_pages(PGD_FOR_VADDR(vaddr, table), vaddr, npages);
}

vmblock_t mm_copy_maps(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages)
{
    if (unlikely(from.pgd == 0 || to.pgd == 0))
    {
        mos_warn("cannot copy maps from " PTR_FMT " to " PTR_FMT ", pagetable is null", fvaddr, tvaddr);
        return (vmblock_t){ .vaddr = 0, .npages = 0 };
    }

    pagemap_mark_used(to.um_page_map, tvaddr, npages);
    vmblock_t block = platform_mm_copy_maps(PGD_FOR_VADDR(fvaddr, from), fvaddr, PGD_FOR_VADDR(tvaddr, to), tvaddr, npages);
    block.address_space = to;
    return block;
}

bool mm_get_is_mapped(paging_handle_t table, uintptr_t vaddr)
{
    if (unlikely(table.pgd == 0))
    {
        mos_warn("cannot get is mapped at " PTR_FMT ", pagetable is null", vaddr);
        return false;
    }

    const bool is_kernel = vaddr >= MOS_KERNEL_START_VADDR;
    const size_t pagemap_base = is_kernel ? MOS_KERNEL_START_VADDR : 0;
    const size_t pagemap_size_lines = is_kernel ? MOS_PAGEMAP_KERNEL_LINES : MOS_PAGEMAP_USER_LINES;
    const size_t pagemap_index = (vaddr - pagemap_base) / MOS_PAGE_SIZE;

    bitmap_line_t *pagemap = is_kernel ? kernel_page_map : table.um_page_map->ummap;
    return bitmap_get(pagemap, pagemap_size_lines, pagemap_index);
}

vmblock_t mm_get_block_info(paging_handle_t table, uintptr_t vaddr, size_t n_pages)
{
    if (unlikely(table.pgd == 0))
    {
        mos_warn("cannot get block info at " PTR_FMT ", pagetable is null", vaddr);
        return (vmblock_t){ .vaddr = 0, .npages = 0 };
    }

    vaddr = vaddr & ~(MOS_PAGE_SIZE - 1);
    const bool is_kernel = vaddr >= MOS_KERNEL_START_VADDR;
    const size_t pagemap_base = is_kernel ? MOS_KERNEL_START_VADDR : 0;
    const size_t pagemap_size_lines = is_kernel ? MOS_PAGEMAP_KERNEL_LINES : MOS_PAGEMAP_USER_LINES;
    const size_t pagemap_index = (vaddr - pagemap_base) / MOS_PAGE_SIZE;

    bitmap_line_t *pagemap = is_kernel ? kernel_page_map : table.um_page_map->ummap;
    MOS_ASSERT_X(bitmap_get(pagemap, pagemap_size_lines, pagemap_index), "page %zu is not allocated", pagemap_index);

    return platform_mm_get_block_info(PGD_FOR_VADDR(vaddr, table), vaddr, n_pages);
}

paging_handle_t mm_create_user_pgd(void)
{
    paging_handle_t table = platform_mm_create_user_pgd();
    table.um_page_map = (page_map_t *) kzalloc(sizeof(page_map_t));
    table.pgd_lock = (spinlock_t *) kzalloc(sizeof(spinlock_t));

    if (unlikely(table.pgd == 0 || table.um_page_map == 0 || table.pgd_lock == 0))
    {
        mos_warn("cannot create user pgd");
        if (table.um_page_map != 0)
            kfree(table.um_page_map);
        if (table.pgd_lock != 0)
            kfree(table.pgd_lock);
        if (table.pgd != 0)
            mm_destroy_user_pgd(table);
        return (paging_handle_t){ 0 };
    }

    return table;
}

void mm_destroy_user_pgd(paging_handle_t table)
{
    if (unlikely(table.pgd == 0))
    {
        mos_warn("cannot destroy user pgd, pagetable is null");
        return;
    }

    spinlock_acquire(table.pgd_lock);
    spinlock_acquire(&table.um_page_map->lock);

    platform_mm_destroy_user_pgd(table);
    kfree(table.um_page_map);
    kfree(table.pgd_lock);
}
