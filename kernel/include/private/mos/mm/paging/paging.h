// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/bitmap.h"
#include "lib/sync/spinlock.h"
#include "mos/platform/platform.h"

/**
 * @defgroup paging Paging
 * @ingroup mm
 * @brief This module provides a platform-independent interface to interact
 * with the paging subsystem, such as allocating and freeing pages, and
 * mapping and unmapping them.
 *
 * @{
 */

typedef enum
{
    MM_COPY_DEFAULT = 0,       ///< Default copy flags.
    MM_COPY_ASSUME_MAPPED = 1, ///< Unmap the destination pages before copying.
} mm_copy_behavior_t;

/// @brief Maximum 'lines' in a page map, see also @ref bitmap_line_t.
#define MOS_PAGEMAP_MAX_LINES BITMAP_LINE_COUNT((uintptr_t) ~0 / MOS_PAGE_SIZE)

/// @brief Number of lines in the page map for user space.
#define MOS_PAGEMAP_USER_LINES BITMAP_LINE_COUNT(MOS_KERNEL_START_VADDR / MOS_PAGE_SIZE)

/// @brief Number of lines in the page map for kernel space.
#define MOS_PAGEMAP_KERNEL_LINES (MOS_PAGEMAP_MAX_LINES - MOS_PAGEMAP_USER_LINES)

typedef struct _page_map
{
    bitmap_line_t ummap[MOS_PAGEMAP_USER_LINES];
    spinlock_t lock;
} page_map_t;

/**
 * @brief Gets npages unmapped free pages from a page table.
 *
 * @param table The page table to allocate from.
 * @param npages The number of pages to allocate.
 * @param hints Allocation hints, see @ref pgalloc_hints.
 * @return uintptr_t The virtual address of the block of virtual memory.
 *
 * @note This function neither allocates nor maps the pages, it only
 * determines the block of virtual memory that can be used to allocate
 * and map the pages.
 *
 * @note The returned @ref vmblock_t only contains the virtual address
 * and the number of pages, it does not contain any physical addresses,
 * nor the flags of the pages.
 */

uintptr_t mm_get_free_pages(paging_handle_t table, size_t npages, pgalloc_hints hints);

/**
 * @brief Allocate npages pages from a page table.
 *
 * @param table The page table to allocate from.
 * @param npages The number of pages to allocate.
 * @param hints Allocation hints, see @ref pgalloc_hints.
 * @param flags Flags to set on the pages, see @ref vm_flags.
 * @return vmblock_t The allocated block of virtual memory, with the
 * number of pages, physical addresses and flags set.
 *
 * @details This function first finds a block of virtual memory using
 * @ref mm_get_free_pages, then allocates and maps the pages.
 */
vmblock_t mm_alloc_pages(paging_handle_t table, size_t npages, pgalloc_hints hints, vm_flags flags);

/**
 * @brief Allocate npages pages from a page table at a specific virtual address.
 *
 * @param table The page table to allocate from.
 * @param vaddr The virtual address to allocate at.
 * @param npages The number of pages to allocate.
 * @param flags Flags to set on the pages, see @ref vm_flags.
 * @return vmblock_t The allocated block of virtual memory, with the number of pages, physical addresses and flags set.
 */
vmblock_t mm_alloc_pages_at(paging_handle_t table, uintptr_t vaddr, size_t npages, vm_flags flags);

/**
 * @brief Map a block of virtual memory to a block of physical memory.
 *
 * @param table The page table to map to.
 * @param vaddr The virtual address to map to.
 * @param paddr The physical address to map from.
 * @param npages The number of pages to map.
 * @param flags Flags to set on the pages, see @ref vm_flags.
 *
 * @details This function maps the pages in the block, and will increment their reference count.
 *
 * @note This function is rarely used directly, you don't always know the physical address of the
 * pages you want to map.
 *
 * @note You may need to reserve the physical memory before mapping it, see @ref pmm_reserve_frames.
 */
vmblock_t mm_map_pages(paging_handle_t table, uintptr_t vaddr, uintptr_t paddr, size_t npages, vm_flags flags);

/**
 * @brief Unmap and possibly free a block of virtual memory.
 *
 * @param table The page table to free from.
 * @param vaddr The virtual address to free.
 * @param npages The number of pages to free.
 *
 * @details This function unmaps the pages in the block, and returns the corresponding physical memory
 * back to the allocator if their reference count reaches zero.
 */
void mm_unmap_pages(paging_handle_t table, uintptr_t vaddr, size_t npages);

/**
 * @brief Remap a block of virtual memory from one page table to another, i.e. copy the mappings.
 *
 * @param from The page table to copy from.
 * @param fvaddr The virtual address to copy from.
 * @param to The page table to copy to.
 * @param tvaddr The virtual address to copy to.
 * @param npages The number of pages to copy.
 * @param clear_dest Whether to clear the destination page table before copying.
 *
 * @details This function firstly unmaps the pages in the destination page table, then copies the pages
 * mappings, including the flags and physical addresses, the physical page reference count is updated
 * accordingly.
 *
 * @note If clear_dest is set to true, then the destination page table is cleared before copying, otherwise
 * the function assumes that there are no existing mappings in the destination page table.
 */
vmblock_t mm_copy_mapping(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages, mm_copy_behavior_t behavior);

/**
 * @brief Get if a virtual address is mapped in a page table.
 *
 * @param table The page table to get the physical address from.
 * @param vaddr The virtual address to get the physical address of.
 * @return bool True if the virtual address is mapped, false otherwise.
 */
bool mm_get_is_mapped(paging_handle_t table, uintptr_t vaddr);

/**
 * @brief Update the flags of a block of virtual memory.
 *
 * @param table The page table to update the flags in.
 * @param vaddr The virtual address to update the flags of.
 * @param npages The number of pages to update the flags of.
 * @param flags The flags to set on the pages, see @ref vm_flags.
 *
 * @note This function is just a wrapper around @ref platform_mm_flag_pages, with correct locking.
 */
void mm_flag_pages(paging_handle_t table, uintptr_t vaddr, size_t npages, vm_flags flags);

/**
 * @brief Create a user-mode platform-dependent page table.
 * @return paging_handle_t The created page table.
 * @note A platform-independent page-map is also created.
 */
paging_handle_t mm_create_user_pgd(void);

/**
 * @brief Destroy a user-mode platform-dependent page table.
 * @param table The page table to destroy.
 * @note The platform-independent page-map is also destroyed.
 */
void mm_destroy_user_pgd(paging_handle_t table);

/** @} */
