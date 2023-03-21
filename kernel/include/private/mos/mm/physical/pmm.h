// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/lib/structures/list.h>
#include <mos/types.h>

/**
 * @defgroup pmm Physical Memory Manager
 * @ingroup mm
 * @brief Allocate/free the physical memory.
 *
 * @details
 * The physical memory manager is responsible for allocating and freeing physical memory.
 *
 * PMM is initialized by the (probably) early architecture-specific code, where it calls
 * \ref pmm_add_region_bytes to add the physical memory information provided by the bootloader.
 *
 * After enough (typically all) physical memory has been added, \ref pmm_allocate_frames can be
 * called to allocate physical memory.
 *
 * @note The allocated physical memory is not zeroed out, in fact, it does nothing to the real memory.
 *
 * @{
 */

/**
 * @brief A read-only list of unallocated physical memory regions, sorted by the physical address.
 */
extern const list_head *const pmlist_free;

/**
 * @brief A read-only list of allocated physical memory regions, sorted by the physical address.
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
 * @brief Callback function type for \ref pmm_allocate_frames.
 *
 * @param op_state Pointer to the operation state structure, \ref pmm_op_state_t
 * @param current Pointer to the current block
 * @param arg Pointer to the argument passed to \ref pmm_allocate_frames
 *
 */
typedef void (*pmm_allocate_callback_t)(const pmm_op_state_t *op_state, const pmrange_t *current, void *arg);

/**
 * @brief Switch to the kernel heap.
 *
 * @note This function should be called after the kernel heap has been setup, and should only be called once.
 */
void pmm_switch_to_kheap(void);

/**
 * @brief Dump the physical memory manager's state, (i.e. the free list and the allocated list).
 */
void pmm_dump_lists(void);

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
 * @brief Allocate blocks of physical memory.
 *
 * @param n_pages Number of pages to allocate.
 * @param callback Callback function to be called for each block of physical memory allocated.
 * @param arg Argument to be passed to the callback function.
 *
 * @return true if the operation succeeded, false otherwise.
 *
 * @note The callback function will be called for each block of physical memory allocated.
 * At the time of the callback, the block will be in the allocated list, marked as `PMM_REGION_ALLOCATED`
 * and have a reference count of 1, so the callback does not need to increase the reference count again.
 *
 * @note The allocation is not guaranteed to be contiguous.
 */
__nodiscard bool pmm_allocate_frames(size_t n_pages, pmm_allocate_callback_t callback, void *arg);

/**
 * @brief Increase the reference count of a list of blocks of physical memory.
 *
 * @param paddr Physical address of the block to reference.
 * @param npages Number of pages to reference.
 *
 * @note Only allocated blocks can be referenced, if one wants to reference a reserved block,
 * use \ref pmm_reserve_frames or \ref pmm_reserve_block first to reserve the block, which only
 * needs to be done once.
 *
 */
void pmm_ref_frames(uintptr_t paddr, size_t npages);

/**
 * @brief Unreference a list of blocks of physical memory.
 *
 * @param paddr Physical address of the block to unreference.
 * @param npages Number of pages to unreference.
 *
 * @note If the reference count of the block reaches 0, the block will be freed and ready to
 * be re-allocated in the future.
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
 * @note If the range does not exist in the free list and overlaps with an allocated block,
 * the kernel panics.
 *
 */
uintptr_t pmm_reserve_frames(uintptr_t paddr, size_t npages);

/**
 * @brief Mark a block of physical memory as reserved.
 *
 * @param needle Pointer to the region to search for.
 *
 * @return pmrange_t The region, if found, otherwise an empty region.
 *
 * @note The memory will be marked as `PMM_REGION_RESERVED` and will be moved to the allocated list.
 */
pmrange_t pmm_reserve_block(uintptr_t needle);

/** @} */
