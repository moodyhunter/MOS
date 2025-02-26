// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mm.hpp"

#include <mos/lib/structures/bitmap.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/platform/platform.hpp>

/**
 * @defgroup paging Paging
 * @ingroup mm
 * @brief A platform-independent interface to map/unmap virtual memory to physical memory.
 *
 * @{
 */

/**
 * @brief Gets npages unmapped free pages from a page table.
 *
 * @param mmctx The memory management context to allocate from.
 * @param n_pages The number of pages to allocate.
 * @param base_vaddr The base virtual address to allocate at.
 * @param exact If the address must be exact.
 * @return ptr_t The virtual address of the block of virtual memory.
 *
 * @note This function neither allocates nor maps the pages, it only
 * determines the block of virtual memory that can be used to allocate
 * and map the pages.
 *
 * @note The returned @ref vmap_t only contains the virtual address
 * and the number of pages, it does not contain any physical addresses,
 * nor the flags of the pages.
 *
 * @warning Should call with mmctx->mm_lock held.
 */

PtrResult<vmap_t> mm_get_free_vaddr_locked(MMContext *mmctx, size_t n_pages, ptr_t base_vaddr, bool exact);

/**
 * @brief Map a block of virtual memory to a block of physical memory.
 *
 * @param mmctx The memory management context to map to.
 * @param vaddr The virtual address to map to.
 * @param pfn The physical frame to map from.
 * @param npages The number of pages to map.
 * @param flags Flags to set on the pages, see @ref VMFlags.
 * @param exact If the address must be exact.
 *
 * @details This function maps the pages in the block, their reference count will NOT be incremented.
 *
 * @note This function is rarely used directly, you don't always know the physical address of the
 * pages you want to map.
 */
void mm_map_kernel_pages(MMContext *mmctx, ptr_t vaddr, pfn_t pfn, size_t npages, VMFlags flags);
PtrResult<vmap_t> mm_map_user_pages(MMContext *mmctx, ptr_t vaddr, pfn_t pfn, size_t npages, VMFlags flags, vmap_type_t type, vmap_content_t content, bool exact = false);

/**
 * @brief Replace the mappings of a page with a new physical frame.
 *
 * @param mmctx The memory management context to replace in.
 * @param vaddr The virtual address to replace.
 * @param pfn The physical frame to replace to.
 * @param flags The new flags to set on the pages.
 *
 * @note The reference count of the physical frame will be incremented, and the reference count of the
 * old physical frame will be decremented.
 */
void mm_replace_page_locked(MMContext *mmctx, ptr_t vaddr, pfn_t pfn, VMFlags flags);

/**
 * @brief Remap a block of virtual memory from one page table to another, i.e. copy the mappings.
 *
 * @param src_vmap The source vmap
 * @param dst_ctx The destination paging context
 *
 * @details This function firstly unmaps the pages in the destination page table, then copies the pages
 * mappings, including the flags and physical addresses, the physical page reference count is updated
 * accordingly.
 *
 * @note If clear_dest is set to true, then the destination page table is cleared before copying, otherwise
 * the function assumes that there are no existing mappings in the destination page table.
 */
PtrResult<vmap_t> mm_clone_vmap_locked(const vmap_t *src_vmap, MMContext *dst_ctx);

/**
 * @brief Get if a virtual address is mapped in a page table.
 *
 * @param mmctx The memory management context to check in.
 * @param vaddr The virtual address to get the physical address of.
 * @return bool True if the virtual address is mapped, false otherwise.
 */
bool mm_get_is_mapped_locked(MMContext *mmctx, ptr_t vaddr);

/**
 * @brief Update the flags of a block of virtual memory.
 *
 * @param mmctx The memory management context to update the flags in.
 * @param vaddr The virtual address to update the flags of.
 * @param npages The number of pages to update the flags of.
 * @param flags The flags to set on the pages, see @ref VMFlags.
 */
void mm_flag_pages_locked(MMContext *mmctx, ptr_t vaddr, size_t npages, VMFlags flags);

ptr_t mm_get_phys_addr(MMContext *ctx, ptr_t vaddr);

/** @} */
