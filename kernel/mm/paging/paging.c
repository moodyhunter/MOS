// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/paging/paging.h"

#include "lib/structures/bitmap.h"
#include "lib/sync/spinlock.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/physical/pmm.h"
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

// ! BEGIN: CALLBACKS
static void vmm_iterate_unmap_novfree(const pgt_iteration_info_t *iter_info, const vmblock_t *block, uintptr_t block_paddr, void *arg)
{
    MOS_UNUSED(iter_info);
    MOS_ASSERT(arg == NULL);
    mos_debug(vmm_impl, "unmapping novfree" PTR_FMT " -> " PTR_FMT " (npages: %zu)", block->vaddr, block_paddr, block->npages);
    pmm_unref_frames(block_paddr, block->npages);
    platform_mm_unmap_pages(PGD_FOR_VADDR(block->vaddr, block->address_space), block->vaddr, block->npages);
}

static void vmm_iterate_unmap(const pgt_iteration_info_t *iter_info, const vmblock_t *block, uintptr_t block_paddr, void *arg)
{
    MOS_UNUSED(iter_info);
    MOS_ASSERT(arg == NULL);
    mos_debug(vmm_impl, "unmapping " PTR_FMT " -> " PTR_FMT " (npages: %zu)", block->vaddr, block_paddr, block->npages);
    pagemap_mark_free(block->address_space.um_page_map, block->vaddr, block->npages);
    pmm_unref_frames(block_paddr, block->npages);
    platform_mm_unmap_pages(PGD_FOR_VADDR(block->vaddr, block->address_space), block->vaddr, block->npages);
}

static void vmm_iterate_copymap(const pgt_iteration_info_t *iter_info, const vmblock_t *block, uintptr_t block_paddr, void *arg)
{
    vmblock_t *vblock = arg;
    if (!vblock->flags)
        vblock->flags = block->flags;
    else
        MOS_ASSERT_X(vblock->flags == block->flags, "flags mismatch");
    const uintptr_t target_vaddr = vblock->vaddr + (block->vaddr - iter_info->vaddr_start); // we know that vaddr is contiguous, so their difference is the offset
    mos_debug(vmm_impl, "copymapping " PTR_FMT " -> " PTR_FMT " (npages: %zu)", target_vaddr, block_paddr, block->npages);
    pmm_ref_frames(block_paddr, block->npages);
    platform_mm_map_pages(PGD_FOR_VADDR(target_vaddr, vblock->address_space), target_vaddr, block_paddr, block->npages, block->flags);
}

static void vmm_handle_pmalloc(const pmm_op_state_t *op_state, const pmrange_t *current, void *arg)
{
    const vmblock_t *vblock = arg;
    const uintptr_t vaddr = vblock->vaddr + op_state->pages_operated * MOS_PAGE_SIZE;
    mos_debug(vmm_impl, "allocating " PTR_FMT " -> " PTR_FMT " (npages: %zu)", vaddr, current->paddr, current->npages);
    platform_mm_map_pages(PGD_FOR_VADDR(vaddr, vblock->address_space), vaddr, current->paddr, current->npages, vblock->flags);
}
// ! END: CALLBACKS

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

    vmblock_t block = { .address_space = table, .vaddr = vaddr, .npages = n_pages, .flags = flags };

    table = PGD_FOR_VADDR(vaddr, table);

    if (unlikely(table.pgd == 0))
    {
        mos_warn("cannot allocate %zd pages at " PTR_FMT ", pagetable is null", n_pages, vaddr);
        return (vmblock_t){ 0 };
    }

    spinlock_acquire(table.pgd_lock);
    pagemap_mark_used(table.um_page_map, vaddr, n_pages);
    const bool result = pmm_allocate_frames(n_pages, vmm_handle_pmalloc, &block);
    spinlock_release(table.pgd_lock);

    if (unlikely(!result))
    {
        mos_warn("could not allocate %zd physical pages", n_pages);
        return (vmblock_t){ 0 };
    }

    return block;
}

vmblock_t mm_map_pages(paging_handle_t table, uintptr_t vaddr, uintptr_t paddr, size_t npages, vm_flags flags)
{
    MOS_ASSERT(npages > 0);

    const vmblock_t block = { .address_space = table, .vaddr = vaddr, .npages = npages, .flags = flags };

    table = PGD_FOR_VADDR(vaddr, table); // after block is initialized

    if (unlikely(table.pgd == 0))
    {
        mos_warn("cannot map pages at " PTR_FMT ", pagetable is null", vaddr);
        return (vmblock_t){ 0 };
    }

    mos_debug(vmm, "mapping %zd pages at " PTR_FMT " to " PTR_FMT, npages, vaddr, paddr);

    spinlock_acquire(table.pgd_lock);
    pagemap_mark_used(table.um_page_map, vaddr, npages);
    pmm_ref_frames(paddr, npages);
    platform_mm_map_pages(table, vaddr, paddr, npages, flags);
    spinlock_release(table.pgd_lock);

    return block;
}

void mm_unmap_pages(paging_handle_t table, uintptr_t vaddr, size_t npages)
{
    MOS_ASSERT(npages > 0);

    table = PGD_FOR_VADDR(vaddr, table);
    spinlock_acquire(table.pgd_lock);
    platform_mm_iterate_table(table, vaddr, npages, vmm_iterate_unmap, NULL);
    spinlock_release(table.pgd_lock);
}

vmblock_t mm_copy_maps(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages, mm_copy_behavior_t behavior)
{
    MOS_ASSERT(npages > 0);
    mos_debug(vmm, "copying mapping from " PTR_FMT " to " PTR_FMT ", %zu pages", fvaddr, tvaddr, npages);

    from = PGD_FOR_VADDR(fvaddr, from);
    vmblock_t result = { .address_space = to, .vaddr = tvaddr, .npages = npages };
    to = PGD_FOR_VADDR(tvaddr, to); // after result is initialized

    if (unlikely(from.pgd == 0 || to.pgd == 0))
    {
        mos_warn("cannot remap pages from " PTR_FMT " to " PTR_FMT ", pagetable is null", fvaddr, tvaddr);
        return (vmblock_t){ 0 };
    }

    spinlock_acquire(from.pgd_lock);
    if (to.pgd_lock != from.pgd_lock)
        spinlock_acquire(to.pgd_lock);

    // 1. unmap pages in target, but don't free the vaddr space (if behavior == MM_COPY_UNMAP_FIRST)
    // 2. copy mappings from source to target
    // TODO: this operation is not atomic, so there MIGHT be a possibility that while we have "unmapped"
    // TODO: the pages, another thread issues a page fault and... idk what happens then.
    if (unlikely(behavior == MM_COPY_ASSUME_MAPPED))
        platform_mm_iterate_table(to, tvaddr, npages, vmm_iterate_unmap_novfree, NULL);
    else
        pagemap_mark_used(to.um_page_map, tvaddr, npages);

    platform_mm_iterate_table(from, fvaddr, npages, vmm_iterate_copymap, &result);

    if (to.pgd_lock != from.pgd_lock)
        spinlock_release(to.pgd_lock);
    spinlock_release(from.pgd_lock);

    return result;
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

    const bitmap_line_t *pagemap = is_kernel ? kernel_page_map : table.um_page_map->ummap;
    return bitmap_get(pagemap, pagemap_size_lines, pagemap_index);
}

void mm_flag_pages(paging_handle_t table, uintptr_t vaddr, size_t npages, vm_flags flags)
{
    MOS_ASSERT(npages > 0);

    table = PGD_FOR_VADDR(vaddr, table);
    if (unlikely(table.pgd == 0))
    {
        mos_warn("cannot flag pages at " PTR_FMT ", pagetable is null", vaddr);
        return;
    }

    mos_debug(vmm, "flagging %zd pages at " PTR_FMT " with flags %x", npages, vaddr, flags);

    spinlock_acquire(table.pgd_lock);
    platform_mm_flag_pages(table, vaddr, npages, flags);
    spinlock_release(table.pgd_lock);
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
