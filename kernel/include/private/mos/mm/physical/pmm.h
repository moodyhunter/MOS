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
    PM_RANGE_UNINITIALIZED = 0, // intentionally 0
    PM_RANGE_FREE = 1,
    PM_RANGE_RESERVED = 2,
    PM_RANGE_ALLOCATED = 3,
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
 * @param op_state Pointer to the operation state structure, \ref pmm_op_state_t
 * @param current Pointer to the current block
 * @param arg Pointer to the argument passed to \ref pmm_ref_new_frames
 *
 */
typedef void (*pmm_op_callback_t)(const pmm_op_state_t *op_state, const pmrange_t *current, void *arg);

/**
 * @brief Switch to the kernel heap.
 *
 * @note This function should be called after the kernel heap has been setup, and should only be called once.
 */
void pmm_switch_to_kheap(void);

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
void pmm_add_region_bytes(uintptr_t start_addr, size_t nbytes, pm_range_type_t type);

/**
 * @brief Allocate a list of blocks of physical memory.
 *
 * @param n_pages Number of pages to allocate.
 * @param callback Callback function to be called for each block of physical memory allocated.
 * @param arg Argument to be passed to the callback function.
 *
 * @return true if the operation succeeded, false otherwise.
 *
 * @note The callback function will be called for each block of physical memory allocated.
 * At the time of the callback, the block will have been marked as `PMM_REGION_ALLOCATED` and
 * will have a reference count of 1.
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
 * @brief Mark a range of physical memory as reserved.
 *
 * @param paddr Physical address of the block to reserve.
 * @param npages Number of pages to reserve.
 * @return uintptr_t The paddr argument, for convenience.
 *
 * @note The memory will be marked as `PMM_REGION_RESERVED` and will be moved to the allocated list.
 *
 * @note If the requested memory cannot be found in the free list, the function will panic.
 */
uintptr_t pmm_reserve_frames(uintptr_t paddr, size_t npages);

/**
 * @brief Mark a block of physical memory as reserved.
 *
 * @param needle Pointer to the region to search for.
 *
 * @return pmrange_t The region
 *
 * @note The memory will be marked as `PMM_REGION_RESERVED` and will be moved to the allocated list.
 */
pmrange_t pmm_reserve_block(uintptr_t needle);

/** @} */
