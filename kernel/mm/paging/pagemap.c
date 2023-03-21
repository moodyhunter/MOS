// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mm/paging/pagemap.h>
#include <mos/mm/paging/paging.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>

static bitmap_line_t kernel_page_map[MOS_PAGEMAP_KERNEL_LINES] = { 0 };
static spinlock_t kernel_page_map_lock = SPINLOCK_INIT;

void pagemap_mark_used(page_map_t *map, uintptr_t vaddr, size_t n_pages)
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

    bitmap_line_t *pagemap = is_kernel ? kernel_page_map : map->ummap;
    spinlock_t *lock = is_kernel ? &kernel_page_map_lock : &map->lock;
    MOS_ASSERT_X(pagemap && lock, "pagemap or lock is NULL");

    spinlock_acquire(lock);
    for (size_t i = 0; i < n_pages; i++)
        if (!bitmap_set(pagemap, pagemap_size_lines, pagemap_index + i))
            mos_panic("page " PTR_FMT " is already used", pagemap_base + (uintptr_t) (pagemap_index + i) * MOS_PAGE_SIZE);
    spinlock_release(lock);
}

void pagemap_mark_free(page_map_t *map, uintptr_t vaddr, size_t n_pages)
{
    const bool is_kernel = vaddr >= MOS_KERNEL_START_VADDR;
    const uintptr_t pagemap_base = is_kernel ? MOS_KERNEL_START_VADDR : 0;
    const size_t pagemap_size_lines = is_kernel ? MOS_PAGEMAP_KERNEL_LINES : MOS_PAGEMAP_USER_LINES;
    const size_t pagemap_index = (vaddr - pagemap_base) / MOS_PAGE_SIZE;

    bitmap_line_t *pagemap = is_kernel ? kernel_page_map : map->ummap;
    spinlock_t *lock = is_kernel ? &kernel_page_map_lock : &map->lock;
    MOS_ASSERT_X(pagemap && lock, "pagemap or lock is NULL");

    spinlock_acquire(lock);
    for (size_t i = 0; i < n_pages; i++)
        if (!bitmap_clear(pagemap, pagemap_size_lines, pagemap_index + i))
            mos_panic("page " PTR_FMT " is already free", pagemap_base + (uintptr_t) (pagemap_index + i) * MOS_PAGE_SIZE);
    spinlock_release(lock);
}

uintptr_t mm_get_free_pages(paging_handle_t table, size_t n_pages, uintptr_t base_vaddr, valloc_flags flags)
{
    const bool is_kernel = base_vaddr >= MOS_KERNEL_START_VADDR;
    const uintptr_t pagemap_base = is_kernel ? MOS_KERNEL_START_VADDR : 0;
    const size_t pagemap_size_lines = is_kernel ? MOS_PAGEMAP_KERNEL_LINES : MOS_PAGEMAP_USER_LINES;
    const size_t pagemap_index = (base_vaddr - pagemap_base) / MOS_PAGE_SIZE;

    bitmap_line_t *map = is_kernel ? kernel_page_map : table.um_page_map->ummap;
    spinlock_t *lock = is_kernel ? &kernel_page_map_lock : &table.um_page_map->lock;

    spinlock_acquire(lock);

    size_t page_i = pagemap_index;

    if (!(flags & VALLOC_EXACT))
    {
        page_i = bitmap_find_first_free_n(map, pagemap_size_lines, pagemap_index, n_pages);
        if (unlikely(page_i == 0))
        {
            spinlock_release(lock);
            pr_warn("no contiguous %zu pages found in pagemap", n_pages);
            return 0;
        }
    }

    for (size_t i = 0; i < n_pages; i++)
        if (!bitmap_set(map, pagemap_size_lines, page_i + i))
            mos_panic("page " PTR_FMT " is already used", pagemap_base + (uintptr_t) (page_i + i) * MOS_PAGE_SIZE);
    spinlock_release(lock);

    return pagemap_base + page_i * MOS_PAGE_SIZE;
}

bool pagemap_get_mapped(page_map_t *map, uintptr_t vaddr)
{
    const bool is_kernel = vaddr >= MOS_KERNEL_START_VADDR;
    const uintptr_t pagemap_base = is_kernel ? MOS_KERNEL_START_VADDR : 0;
    const size_t pagemap_size_lines = is_kernel ? MOS_PAGEMAP_KERNEL_LINES : MOS_PAGEMAP_USER_LINES;
    const size_t pagemap_index = (vaddr - pagemap_base) / MOS_PAGE_SIZE;

    bitmap_line_t *pagemap = is_kernel ? kernel_page_map : map->ummap;
    spinlock_t *lock = is_kernel ? &kernel_page_map_lock : &map->lock;
    MOS_ASSERT_X(pagemap && lock, "pagemap or lock is NULL");

    spinlock_acquire(lock);
    const bool mapped = bitmap_get(pagemap, pagemap_size_lines, pagemap_index);
    spinlock_release(lock);

    return mapped;
}
