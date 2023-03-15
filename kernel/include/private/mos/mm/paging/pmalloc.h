// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/list.h"
#include "mos/types.h"

/**
 * @defgroup Physical Memory Manager
 * @ingroup mm
 * @brief The physical memory manager (pmm) is responsible for managing physical memory.
 * @{
 */

/**
 * @brief A read-only list of physical memory regions provided by the bootloader.
 */
extern const list_head *const pmlist_free;

/**
 * @brief A read-only list of allocated physical memory regions.
 */
extern const list_head *const pmlist_allocated;

typedef enum
{
    PMM_REGION_UNINITIALIZED = 0, // intentionally 0
    PMM_REGION_FREE = 1,
    PMM_REGION_RESERVED = 2,
    PMM_REGION_ALLOCATED = 3,
} pm_range_type_t;

/**
 * @brief A range of physical memory.
 *
 * @details There might be multiple ranges of physical memory that are allocated
 * in one allocation, users of this structure is expected to know
 *
 */
typedef struct
{
    uintptr_t paddr;
    size_t npages;
} pmrange_t;

typedef struct
{
    size_t pages_requested; // number of pages requested by the user
    size_t pages_operated;  // excluding the current block (the one being passed to the callback)
} pmm_op_state_t;

/**
 * @brief Callback function type for \ref pmm_ref_new_frames.
 *
 * @param i Index of the current block
 * @param op_state Pointer to the operation state structure, \ref pmm_op_state_t
 * @param current Pointer to the current block
 * @param arg Pointer to the argument passed to \ref pmm_ref_new_frames
 *
 */
typedef void (*pmm_op_callback_t)(size_t i, const pmm_op_state_t *op_state, const pmrange_t *current, void *arg);

/**
 * @brief Dump the physical memory manager's state, (i.e. the free list)
 */
void pmm_dump(void);

/**
 * @brief Add a region of physical memory to the physical memory manager.
 *
 * @param start_addr Starting address of the region
 * @param nbytes Size of the region in bytes
 * @param type Type of the region
 *
 * @note The second parameter is in bytes, not pages, page-aligning will be done internally.
 *
 */
void mos_pmm_add_region(uintptr_t start_addr, size_t nbytes, pm_range_type_t type);

/**
 * @brief Switch to the kernel heap.
 *
 * @note This function should be called after the kernel heap has been setup, and should only be called once.
 */
void pmm_switch_to_kheap(void);

/**
 * @brief Allocate a list of blocks of physical memory.
 *
 * @param n_pages Number of pages to allocate.
 * @param callback Callback function to be called for each block of physical memory allocated.
 * @param arg Argument to be passed to the callback function.
 *
 * @return true if the operation succeeded, false otherwise.
 */
__nodiscard bool pmm_allocate_frames(size_t n_pages, pmm_op_callback_t callback, void *arg);

/**
 * @brief Increase the reference count of a list of blocks of physical memory.
 *
 * @param paddr Physical address of the block to reference.
 * @param npages Number of pages to reference.
 *
 */
void pmm_ref_frames(uintptr_t paddr, size_t npages);

/**
 * @brief Unreference a list of blocks of physical memory.
 *
 * @param paddr Physical address of the block to unreference.
 * @param npages Number of pages to unreference.
 *
 */
void pmm_unref_frames(uintptr_t paddr, size_t npages);

/**
 * @brief Mark a block of physical memory as reserved.
 *
 * @param paddr Physical address of the block to reserve.
 * @param npages Number of pages to reserve.
 *
 * @note The memory will be marked as `PMM_REGION_RESERVED` and will be ignored by \ref pmm_allocate_frames.
 */
void pmm_reserve_frames(uintptr_t paddr, size_t npages);

/**
 * @brief Get a reserved region of physical memory.
 *
 * @param needle Pointer to the region to search for.
 *
 * @return pmrange_t The reserved region
 *
 * @note The returned region is moved from the free list to the allocated list, with a
 * reference count of 1.
 */
pmrange_t pmm_ref_reserved_region(uintptr_t needle);

/** @} */
