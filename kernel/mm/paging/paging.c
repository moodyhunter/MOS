// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/paging.h"

#include "lib/structures/bitmap.h"
#include "lib/sync/spinlock.h"
#include "mos/constants.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/paging/pmalloc.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"

#define PGD_FOR_VADDR(_vaddr, _um) (_vaddr >= MOS_KERNEL_START_VADDR ? platform_info->kernel_pgd : _um)

static bitmap_line_t kernel_page_map[MOS_PAGEMAP_KERNEL_LINES] = { 0 };
static spinlock_t kernel_page_map_lock = SPINLOCK_INIT;

static void pagemap_mark_used(page_map_t *map, uintptr_t vaddr, size_t n_pages)
{
    const bool is_kernel = vaddr >= MOS_KERNEL_START_VADDR;
    const uintptr_t pagemap_base = is_kernel ? MOS_KERNEL_START_VADDR : 0;
    const size_t pagemap_size_lines = is_kernel ? MOS_PAGEMAP_KERNEL_LINES : MOS_PAGEMAP_USER_LINES;
    const size_t pagemap_index = (vaddr - pagemap_base) / MOS_PAGE_SIZE;

    bitmap_line_t *pagemap = is_kernel ? kernel_page_map : map->ummap;
    spinlock_t *lock = is_kernel ? &kernel_page_map_lock : &map->lock;

    if (unlikely(pagemap_index + n_pages > pagemap_size_lines * BITMAP_LINE_BITS))
    {
        mos_panic("pagemap_use: out of bounds");
    }

    spinlock_acquire(lock);
    for (size_t i = 0; i < n_pages; i++)
    {
        bool is_set = bitmap_get(pagemap, pagemap_size_lines, pagemap_index + i);
        MOS_ASSERT_X(!is_set, "page " PTR_FMT " is already allocated", (pagemap_index + i) * MOS_PAGE_SIZE + pagemap_base);
        bitmap_set(pagemap, pagemap_size_lines, pagemap_index + i);
        is_set = bitmap_get(pagemap, pagemap_size_lines, pagemap_index + i);
        MOS_ASSERT_X(is_set, "page " PTR_FMT " is not set", (pagemap_index + i) * MOS_PAGE_SIZE + pagemap_base);
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

    if (unlikely(pagemap_index + n_pages > pagemap_size_lines * BITMAP_LINE_BITS))
    {
        mos_panic("pagemap_free: out of bounds");
    }

    spinlock_acquire(lock);
    for (size_t i = 0; i < n_pages; i++)
    {
        bool is_set = bitmap_get(pagemap, pagemap_size_lines, pagemap_index + i);
        MOS_ASSERT_X(is_set, "page " PTR_FMT " is already free", (pagemap_index + i) * MOS_PAGE_SIZE + pagemap_base);
        bitmap_clear(pagemap, pagemap_size_lines, pagemap_index + i);
    }
    spinlock_release(lock);
}

vmblock_t mm_get_free_pages(paging_handle_t table, size_t n_pages, pgalloc_hints hints)
{
    static const uintptr_t limits[] = {
        [PGALLOC_HINT_KHEAP] = MOS_ADDR_KERNEL_HEAP,
        [PGALLOC_HINT_UHEAP] = MOS_ADDR_USER_HEAP,
        [PGALLOC_HINT_STACK] = MOS_ADDR_USER_STACK,
        [PGALLOC_HINT_MMAP] = MOS_ADDR_USER_MMAP,
    };

    uintptr_t vaddr_begin = limits[hints];

    const bool is_kernel = hints == PGALLOC_HINT_KHEAP;
    bitmap_line_t *pagemap = is_kernel ? kernel_page_map : table.um_page_map->ummap;
    spinlock_t *lock = is_kernel ? &kernel_page_map_lock : &table.um_page_map->lock;
    const uintptr_t pagemap_base = is_kernel ? MOS_KERNEL_START_VADDR : 0;
    const size_t pagemap_size_lines = is_kernel ? MOS_PAGEMAP_KERNEL_LINES : MOS_PAGEMAP_USER_LINES;
    const size_t pagemap_begin_index = (vaddr_begin - pagemap_base) / MOS_PAGE_SIZE;

    spinlock_acquire(lock);
    size_t page_i = bitmap_find_first_free_n(pagemap, pagemap_size_lines, pagemap_begin_index, n_pages);
    MOS_ASSERT_X(!bitmap_get(pagemap, pagemap_size_lines, page_i), "page %zu is already allocated", page_i);
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

    return (vmblock_t){ .vaddr = vaddr, .npages = n_pages };
}

vmblock_t mm_alloc_pages(paging_handle_t table, size_t npages, pgalloc_hints hints, vm_flags flags)
{
    vmblock_t block = mm_get_free_pages(table, npages, hints);
    return mm_alloc_pages_at(table, block.vaddr, block.npages, flags);
}

vmblock_t mm_alloc_pages_at(paging_handle_t table, uintptr_t vaddr, size_t n_pages, vm_flags flags)
{
    uintptr_t paddr = pmalloc_alloc(n_pages);
    MOS_ASSERT_X(paddr != 0, "could not find free physical memory");

    vmblock_t block = { .vaddr = vaddr, .npages = n_pages, .paddr = paddr, .flags = flags };
    mm_map_allocated_pages(table, block);
    return block;
}

void mm_free_pages(paging_handle_t table, vmblock_t block)
{
    mm_unmap_pages(table, block.vaddr, block.npages);
    pmalloc_release_region(block.paddr, block.npages * MOS_PAGE_SIZE);
}

void mm_map_pages(paging_handle_t table, vmblock_t block)
{
    pmalloc_acquire_region(block.paddr, block.npages * MOS_PAGE_SIZE);
    mm_map_allocated_pages(table, block);
}

void mm_map_allocated_pages(paging_handle_t table, vmblock_t block)
{
    pagemap_mark_used(table.um_page_map, block.vaddr, block.npages);
    platform_mm_map_pages(PGD_FOR_VADDR(block.vaddr, table), block);
}

void mm_unmap_pages(paging_handle_t table, uintptr_t vaddr, size_t npages)
{
    pagemap_mark_free(table.um_page_map, vaddr, npages);
    platform_mm_unmap_pages(PGD_FOR_VADDR(vaddr, table), vaddr, npages);
}

vmblock_t mm_copy_maps(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages)
{
    pagemap_mark_used(to.um_page_map, tvaddr, npages);
    vmblock_t block = platform_mm_copy_maps(PGD_FOR_VADDR(fvaddr, from), fvaddr, PGD_FOR_VADDR(tvaddr, to), tvaddr, npages);
    return block;
}

bool mm_get_is_mapped(paging_handle_t table, uintptr_t vaddr)
{
    const bool is_kernel = vaddr >= MOS_KERNEL_START_VADDR;
    const size_t pagemap_base = is_kernel ? MOS_KERNEL_START_VADDR : 0;
    const size_t pagemap_size_lines = is_kernel ? MOS_PAGEMAP_KERNEL_LINES : MOS_PAGEMAP_USER_LINES;
    const size_t pagemap_index = (vaddr - pagemap_base) / MOS_PAGE_SIZE;

    bitmap_line_t *pagemap = is_kernel ? kernel_page_map : table.um_page_map->ummap;
    return bitmap_get(pagemap, pagemap_size_lines, pagemap_index);
}

vmblock_t mm_get_block_info(paging_handle_t table, uintptr_t vaddr, size_t n_pages)
{
    const bool is_kernel = vaddr >= MOS_KERNEL_START_VADDR;
    const size_t pagemap_base = is_kernel ? MOS_KERNEL_START_VADDR : 0;
    const size_t pagemap_size_lines = is_kernel ? MOS_PAGEMAP_KERNEL_LINES : MOS_PAGEMAP_USER_LINES;
    const size_t pagemap_index = (vaddr - pagemap_base) / MOS_PAGE_SIZE;

    bitmap_line_t *pagemap = is_kernel ? kernel_page_map : table.um_page_map->ummap;
    MOS_ASSERT_X(bitmap_get(pagemap, pagemap_size_lines, pagemap_index), "page %zu is not allocated", pagemap_index);

    return platform_mm_get_block_info(PGD_FOR_VADDR(vaddr, table), vaddr, n_pages);
}

paging_handle_t mm_create_user_pgd()
{
    paging_handle_t table = platform_mm_create_user_pgd();
    table.um_page_map = (page_map_t *) kzalloc(sizeof(page_map_t));
    table.pgd_lock = (spinlock_t *) kzalloc(sizeof(spinlock_t));
    return table;
}

void mm_destroy_user_pgd(paging_handle_t table)
{
    platform_mm_destroy_user_pgd(table);
    kfree(table.um_page_map);
}
