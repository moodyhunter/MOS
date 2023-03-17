// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/list.h"
#include "mos/mm/physical/pmm.h"

#ifndef mos_pmm_impl
#error "This file should only be used by the physical memory manager implementation."
#endif

/**
 * @defgroup pmm_impl PMM Internal
 * @ingroup pmm
 * @brief The physical memory manager's internal API.
 *
 * @details
 * This API is only meant to be used by the physical memory manager implementation.
 *
 * There are two lists in PMM, one for free blocks, called \ref pmlist_free, and the
 * other for allocated blocks, called \ref pmlist_allocated.
 *
 * Both lists are sorted by the physical address of the block, in ascending order.
 *
 * @{
 */

/**
 * @brief A node in the physical memory manager's linked list.
 *
 * @details
 * A valid pmlist_node_t can be in exactly one of the following states:
 *
 * - \ref PM_RANGE_FREE
 *
 *  Declared as free by the bootloader and has not yet been allocated or reserved.
 *  Or, it has been freed by the PMM if the reference count reaches 0.
 *  Only stored in the free list, reading its reference count is undefined.
 *
 * - \ref PM_RANGE_ALLOCATED
 *
 *  The block is allocated by the kernel, and is not yet freed.
 *  Only stored in the allocated list, with a reference count greater than 0.
 *
 * - \ref PM_RANGE_RESERVED
 *
 *  The block is reserved by the bootloader, or manually reserved by the kernel.
 *  A block in this state can be both in the free list and the allocated list.
 *
 *  A reserved block can be seen in the **free list** if it is initially reserved by
 *  the bootloader at the start.
 *
 *  A reserved block can be seen in the **allocated list** if it is reserved by calling
 *  \ref pmm_reserve_frames or \ref pmm_reserve_block.
 *
 * - \ref PM_RANGE_UNINITIALIZED
 *
 *  An invalid pmlist_node_t has its `type' field set to this and **must not** in any list.
 *  It is undefined to read any other fields of such an invalid pmlist_node_t.
 *   Nodes with this type can only be seen in \ref pmm_early_storage.
 */
typedef struct
{
    as_linked_list;

    /// The range of physical memory this node represents
    pmrange_t range;

    /// Reference count, only valid for allocated nodes
    atomic_t refcount;

    /// Type of the block, see \ref pm_range_type_t
    pm_range_type_t type;
} pmlist_node_t;

/**
 * @brief A boolean indicating whether the kernel heap is ready to be used.
 *
 * @details
 * This is set to true by the kernel heap implementation when it is ready to be used.
 * The flag is set to false at first, and is set to true by \ref pmm_switch_to_kheap.
 *
 */
extern bool pmm_use_kernel_heap;

// ! ====================================================================== General Internal API

/**
 * @brief Create a new pmlist_node_t.
 *
 * @param start Physical address of the start of the block
 * @param n_pages Number of pages in the block
 * @param type Type of the block, see \ref pm_range_type_t
 * @return pmlist_node_t*
 */
pmlist_node_t *pmm_internal_list_node_create(uintptr_t start, size_t n_pages, pm_range_type_t type);

/**
 * @brief Delete a pmlist_node_t.
 * @param node The node to delete
 */
void pmm_internal_list_node_delete(pmlist_node_t *node);

// ! ====================================================================== Freelist API

/**
 * @defgroup pmm_impl_freelist PMM Freelist
 * @ingroup pmm_impl
 * @brief PMM Internal API for the free list.
 *
 * @{
 */

/**
 * @brief Add a new free block to the free list.
 *
 * @param start Physical address of the start of the block
 * @param n_pages Number of pages in the block
 * @param type Type of the block, see \ref pm_range_type_t
 */
void pmm_internal_add_free_frames(uintptr_t start, size_t n_pages, pm_range_type_t type);

/**
 * @brief Add a new free block to the free list.
 *
 * @param node The node to add
 *
 * @note The node must not be in any list, it also may be freed due to the merge
 * that occurs when adding it to the free list.
 */
void pmm_internal_add_free_frames_node(pmlist_node_t *node);

/**
 * @brief Callback function type for \ref pmm_internal_acquire_free_frames.
 */
typedef void (*pmm_internal_op_callback_t)(const pmm_op_state_t *op_state, pmlist_node_t *node, pmm_allocate_callback_t user_callback, void *user_arg);

/**
 * @brief Allocate blocks of physical memory, and call a callback function for each block allocated.
 *
 * @param n_pages Number of pages to allocate
 * @param callback An internal callback function for each block allocated, for example incrementing the reference count
 * @param user_callback User's callback function for each block allocated, must be called by the internal callback function \p callback
 * @param user_arg User's argument to pass to the user callback function
 * @return true if the allocation was successful
 * @return false if the allocation failed
 */
bool pmm_internal_acquire_free_frames(size_t n_pages, pmm_internal_op_callback_t callback, pmm_allocate_callback_t user_callback, void *user_arg);

/**
 * @brief Allocate a new block of physical memory at a specific address.
 *
 * @param start Physical address to allocate at
 * @param n_pages Number of pages to allocate
 * @return pmlist_node_t* The node containing the block, or NULL if the allocation failed
 *
 * @note The resulting node will be removed from the free list.
 */
pmlist_node_t *pmm_internal_acquire_free_frames_at(uintptr_t start, size_t n_pages);

/**
 * @brief Find a free block of physical memory.
 *
 * @param needle Physical address pointing to inside the block to find
 * @param type Type of the block, see \ref pm_range_type_t
 * @return pmlist_node_t* The node containing the block, or NULL if no such block exists
 *
 * @note The resulting node will be removed from the free list.
 */
pmlist_node_t *pmm_internal_find_and_acquire_block(uintptr_t needle, pm_range_type_t type);

/** @} */

// ! ====================================================================== Allocated List (refcount) API

/**
 * @defgroup pmm_impl_allocatedlist PMM Allocated List
 * @ingroup pmm_impl
 * @brief PMM Internal API for the allocated list.
 *
 * @{
 */

/**
 * @brief Add a new allocated block to the allocated list.
 *
 * @param node The node to add
 */
void pmm_internal_add_node_to_allocated_list(pmlist_node_t *node);

/**
 * @brief Increment the reference count of a block.
 *
 * @param start Physical address of the start of the block
 * @param n_pages Number of pages in the block
 */
void pmm_internal_ref_range(uintptr_t start, size_t n_pages);

typedef void (*pmm_internal_unref_range_callback_t)(pmlist_node_t *node, void *arg);

/**
 * @brief Decrement the reference count of a block.
 *
 * @param start Physical address of the start of the block
 * @param n_pages Number of pages in the block
 * @param callback Callback function to call for each block which the reference count reaches 0
 * @param arg Argument to pass to the callback function
 *
 * @note The callback function will be called with the node removed from the allocated list.
 */
void pmm_internal_unref_range(uintptr_t start, size_t n_pages, pmm_internal_unref_range_callback_t callback, void *arg);

/** @} */

/** @} */
