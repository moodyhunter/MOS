// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/tasks/task_types.h"

#include <mos/kconfig.h>
#include <mos/mm/paging/pagemap.h>
#include <mos/mm/paging/paging.h>
#include <mos/platform/platform.h>
#include <mos/printk.h>

/// @brief Number of lines in the page map for kernel heap.
#define MOS_PAGEMAP_KHEAP_LINES BITMAP_LINE_COUNT((MOS_ADDR_KERNEL_HEAP_END - MOS_ADDR_KERNEL_HEAP) / MOS_PAGE_SIZE)

static bitmap_line_t map[MOS_PAGEMAP_KHEAP_LINES] = { 0 };
static spinlock_t kpagemap_lock;

void kpagemap_mark_used(ptr_t vaddr, size_t n_pages)
{
    MOS_ASSERT_X(vaddr % MOS_PAGE_SIZE == 0, "vaddr is not page aligned");
    if (n_pages == 0)
    {
        mos_warn("pagemap_mark_used: n_pages is 0");
        return;
    }

    spinlock_acquire(&kpagemap_lock);
    const size_t start_index = (vaddr - MOS_ADDR_KERNEL_HEAP) / MOS_PAGE_SIZE;
    for (size_t i = 0; i < n_pages; i++)
        if (!bitmap_set(map, MOS_PAGEMAP_KHEAP_LINES, start_index + i))
            mos_panic("page " PTR_FMT " is already used", MOS_ADDR_KERNEL_HEAP + (ptr_t) (start_index + i) * MOS_PAGE_SIZE);
    spinlock_release(&kpagemap_lock);
}

void kpagemap_mark_free(ptr_t vaddr, size_t n_pages)
{
    spinlock_acquire(&kpagemap_lock);
    const size_t start_index = (vaddr - MOS_ADDR_KERNEL_HEAP) / MOS_PAGE_SIZE;
    for (size_t i = 0; i < n_pages; i++)
        if (!bitmap_clear(map, MOS_PAGEMAP_KHEAP_LINES, start_index + i))
            mos_panic("page " PTR_FMT " is already free", MOS_ADDR_KERNEL_HEAP + (ptr_t) (start_index + i) * MOS_PAGE_SIZE);
    spinlock_release(&kpagemap_lock);
}

ptr_t kpagemap_get_free_pages(size_t n_pages, ptr_t base_vaddr, valloc_flags flags)
{
    spinlock_acquire(&kpagemap_lock);
    size_t index = (base_vaddr - MOS_ADDR_KERNEL_HEAP) / MOS_PAGE_SIZE;
    if (!(flags & VALLOC_EXACT))
    {
        index = bitmap_find_first_free_n(map, MOS_PAGEMAP_KHEAP_LINES, index, n_pages);
        if (unlikely(index == (size_t) -1))
        {
            spinlock_release(&kpagemap_lock);
            pr_warn("no contiguous %zu pages found in pagemap", n_pages);
            return 0;
        }
    }

    for (size_t i = 0; i < n_pages; i++)
        if (!bitmap_set(map, MOS_PAGEMAP_KHEAP_LINES, index + i))
            mos_panic("page " PTR_FMT " is already used", (ptr_t) MOS_ADDR_KERNEL_HEAP + (index + i) * MOS_PAGE_SIZE);
    spinlock_release(&kpagemap_lock);

    return MOS_ADDR_KERNEL_HEAP + index * MOS_PAGE_SIZE;
}

bool kpagemap_get_mapped(ptr_t vaddr)
{
    spinlock_acquire(&kpagemap_lock);
    const size_t start_index = (vaddr - MOS_ADDR_KERNEL_HEAP) / MOS_PAGE_SIZE;
    const bool mapped = bitmap_get(map, MOS_PAGEMAP_KHEAP_LINES, start_index);
    spinlock_release(&kpagemap_lock);
    return mapped;
}
