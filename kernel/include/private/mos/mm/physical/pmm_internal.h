// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/list.h"
#include "mos/mm/physical/pmm.h"

#ifndef mos_pmm_impl
#error "This file should only be used by the physical memory manager implementation."
#endif

/**
 * @brief A node in the physical memory manager's linked list.
 *
 * @details
 * A valid pmlist_node_t can be in exactly one of the following states:
 * - free:         Declared as free by the bootloader and has not yet been allocated or reserved.
 *                 Stored in the free list, reading its reference count is undefined.
 *
 * - allocated:    The block is allocated by the kernel, and is not yet freed.
 *                 Stored in the allocated list, with a reference count initialized to 0.
 *
 * - reserved:     The block is reserved by the bootloader, or manually reserved by the kernel.
 *                 The block is in in the allocated list, and with a reference count initialized to 1,
 *                 so that it is never freed.
 *
 * An invalid pmlist_node_t has its `type' field set to `PMM_REGION_UNINITIALIZED' and is not in any list.
 * It is not safe to read any other fields of an invalid pmlist_node_t, such nodes can only be seen in
 * 'pmm_early_storage'.
 *
 */
typedef struct
{
    as_linked_list;
    pmrange_t range;
    atomic_t refcount;
    pm_range_type_t type;
} pmlist_node_t;

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
typedef void (*pmm_internal_op_callback_t)(const pmm_op_state_t *op_state, pmlist_node_t *node, pmm_op_callback_t user_callback, void *user_arg);

/**
 * @brief Allocate a new block of physical memory.
 *
 * @param n_pages Number of pages to allocate
 * @param callback Callback function to call for each block allocated
 * @param arg Argument to pass to the callback function
 * @return true if the allocation was successful
 * @return false if the allocation failed
 */
bool pmm_internal_acquire_free_frames(size_t n_pages, pmm_internal_op_callback_t callback, pmm_op_callback_t user_callback, void *user_arg);

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

// ! ====================================================================== Allocated List (refcount) API

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
