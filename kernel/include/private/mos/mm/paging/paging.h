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
 * @return vmblock_t The determined block of virtual memory.
 *
 * @note This function neither allocates nor maps the pages, it only
 * determines the block of virtual memory that can be used to allocate
 * and map the pages.
 *
 * @note The returned @ref vmblock_t only contains the virtual address
 * and the number of pages, it does not contain any physical addresses,
 * nor the flags of the pages.
 */

vmblock_t mm_get_free_pages(paging_handle_t table, size_t npages, pgalloc_hints hints);

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
 * @brief Free a block of virtual memory, and return the corresponding physical memory to the allocator.
 *
 * @param table The page table to free from.
 * @param block The block of memory to free.
 *
 * @details This function unmaps and frees the pages in the block, it returns the corresponding physical memory
 * back to the allocator.
 */
void mm_free_pages(paging_handle_t table, vmblock_t block);

/**
 * @brief Map a block of virtual memory to a given block of physical memory, with the physical memory requested from the physical memory allocator.
 *
 * @param table The page table to map in.
 * @param block The block of virtual memory to map.
 *
 * @details This function maps the pages in the block, then try requesting the corresponding physical memory
 * from the physical memory allocator, if the physical memory is not available, the action will fail.
 *
 * @warning This function is rarely used directly, it's only used to map the kernel image.
 */
void mm_map_pages(paging_handle_t table, vmblock_t block);

/**
 * @brief Map a block of virtual memory to a given block of physical memory, without the interaction of the physical memory allocator.
 *
 * @param table The page table to map in.
 * @param block The block of virtual memory to map.
 *
 * @details This function maps the pages in the block, unlike the above @ref mm_map_pages, this function does
 * not request the corresponding physical memory from the physical memory allocator, instead, it assumes that
 * the physical memory is already allocated.
 *
 * @note This function can be used to map the hardware memory to a specific virtual address, as hardware memory is
 * not managed by the physical memory allocator.
 */
void mm_map_allocated_pages(paging_handle_t table, vmblock_t block);

/**
 * @brief Unmap a block of virtual memory.
 *
 * @param table The page table to unmap from.
 * @param vaddr The virtual address to unmap.
 * @param npages The number of pages to unmap.
 *
 * @details This function unmaps the pages in the block, it does not return the corresponding physical memory
 * back to the allocator.
 *
 * @note Imagine that you have shared memory between address space A and B, then B unmaps the shared memory,
 * the physical memory is still used by A, so it's not returned to the allocator.
 */
void mm_unmap_pages(paging_handle_t table, uintptr_t vaddr, size_t npages);

/**
 * @brief Copy a block of virtual memory from one page table to another.
 *
 * @param from The page table to copy from.
 * @param fvaddr The virtual address to copy from.
 * @param to The page table to copy to.
 * @param tvaddr The virtual address to copy to.
 * @param npages The number of pages to copy.
 *
 * @details This function copies the pages mappings, including the flags and physical addresses, from one page
 * table to another.
 *
 * @note This function does not copy the actual data, it's used intensively by CoW, Zero-on-Demand and shared memory.
 */
vmblock_t mm_copy_maps(paging_handle_t from, uintptr_t fvaddr, paging_handle_t to, uintptr_t tvaddr, size_t npages);

/**
 * @brief Get if a virtual address is mapped in a page table.
 *
 * @param table The page table to get the physical address from.
 * @param vaddr The virtual address to get the physical address of.
 * @return bool True if the virtual address is mapped, false otherwise.
 */
bool mm_get_is_mapped(paging_handle_t table, uintptr_t vaddr);

/**
 * @brief Get the block info of a virtual address in a page table.
 *
 * @param table The page table to get the physical address from.
 * @param vaddr The virtual address to get the physical address of.
 * @return vmblock_t The block info of the virtual address.
 * @warning vaddr will be rounded down to the nearest page boundary if it's not page-aligned.
 */
vmblock_t mm_get_block_info(paging_handle_t table, uintptr_t vaddr, size_t n_pages);

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
