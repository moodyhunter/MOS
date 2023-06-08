// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/task_types.h"

#include <mos/kconfig.h>
#include <mos/mm/paging/pagemap.h>
#include <mos/mm/paging/paging.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>

/// @brief Maximum 'lines' in a page map, see also @ref bitmap_line_t.
#define MOS_PAGEMAP_MAX_LINES BITMAP_LINE_COUNT((ptr_t) ~0 / MOS_PAGE_SIZE)

/// @brief Number of lines in the page map for user space.
#define MOS_PAGEMAP_USER_LINES BITMAP_LINE_COUNT(MOS_KERNEL_START_VADDR / MOS_PAGE_SIZE)

/// @brief Number of lines in the page map for kernel space.
#define MOS_PAGEMAP_KERNEL_LINES (MOS_PAGEMAP_MAX_LINES - MOS_PAGEMAP_USER_LINES)

static bitmap_line_t map[MOS_PAGEMAP_KERNEL_LINES];
static spinlock_t kpagemap_lock;

void kpagemap_mark_used(ptr_t vaddr, size_t n_pages)
{
    MOS_ASSERT(vaddr >= MOS_KERNEL_START_VADDR);
    MOS_ASSERT_X(vaddr % MOS_PAGE_SIZE == 0, "vaddr is not page aligned");

    if (n_pages == 0)
    {
        mos_warn("pagemap_mark_used: n_pages is 0");
        return;
    }

    const ptr_t pagemap_base = MOS_KERNEL_START_VADDR;
    const size_t pagemap_size_lines = MOS_PAGEMAP_KERNEL_LINES;
    const size_t pagemap_index = (vaddr - pagemap_base) / MOS_PAGE_SIZE;

    spinlock_acquire(&kpagemap_lock);
    for (size_t i = 0; i < n_pages; i++)
        if (!bitmap_set(map, pagemap_size_lines, pagemap_index + i))
            mos_panic("page " PTR_FMT " is already used", pagemap_base + (ptr_t) (pagemap_index + i) * MOS_PAGE_SIZE);
    spinlock_release(&kpagemap_lock);
}

void kpagemap_mark_free(ptr_t vaddr, size_t n_pages)
{
    MOS_ASSERT(vaddr >= MOS_KERNEL_START_VADDR);
    const ptr_t pagemap_base = MOS_KERNEL_START_VADDR;
    const size_t pagemap_size_lines = MOS_PAGEMAP_KERNEL_LINES;
    const size_t pagemap_index = (vaddr - pagemap_base) / MOS_PAGE_SIZE;

    spinlock_acquire(&kpagemap_lock);
    for (size_t i = 0; i < n_pages; i++)
        if (!bitmap_clear(map, pagemap_size_lines, pagemap_index + i))
            mos_panic("page " PTR_FMT " is already free", pagemap_base + (ptr_t) (pagemap_index + i) * MOS_PAGE_SIZE);
    spinlock_release(&kpagemap_lock);
}

ptr_t kpagemap_get_free_pages(size_t n_pages, ptr_t base_vaddr, valloc_flags flags)
{
    const ptr_t pagemap_base = MOS_KERNEL_START_VADDR;
    const size_t pagemap_size_lines = MOS_PAGEMAP_KERNEL_LINES;
    const size_t pagemap_start_index = (base_vaddr - pagemap_base) / MOS_PAGE_SIZE;

    spinlock_acquire(&kpagemap_lock);
    size_t page_i = pagemap_start_index;

    if (!(flags & VALLOC_EXACT))
    {
        page_i = bitmap_find_first_free_n(map, pagemap_size_lines, pagemap_start_index, n_pages);
        if (unlikely(index == (size_t) -1))
        {
            spinlock_release(&kpagemap_lock);
            pr_warn("no contiguous %zu pages found in pagemap", n_pages);
            return 0;
        }
    }

    for (size_t i = 0; i < n_pages; i++)
        if (!bitmap_set(map, pagemap_size_lines, page_i + i))
            mos_panic("page " PTR_FMT " is already used", pagemap_base + (ptr_t) (page_i + i) * MOS_PAGE_SIZE);
    spinlock_release(&kpagemap_lock);

    return pagemap_base + page_i * MOS_PAGE_SIZE;
}

bool kpagemap_get_mapped(ptr_t vaddr)
{
    MOS_ASSERT(vaddr >= MOS_KERNEL_START_VADDR);
    const ptr_t pagemap_base = MOS_KERNEL_START_VADDR;
    const size_t pagemap_size_lines = MOS_PAGEMAP_KERNEL_LINES;
    const size_t pagemap_start_index = (vaddr - pagemap_base) / MOS_PAGE_SIZE;

    spinlock_acquire(&kpagemap_lock);
    const bool mapped = bitmap_get(map, pagemap_size_lines, pagemap_start_index);
    spinlock_release(&kpagemap_lock);

    return mapped;
}
