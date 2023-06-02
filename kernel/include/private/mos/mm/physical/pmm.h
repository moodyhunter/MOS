// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/lib/sync/spinlock.h"

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

typedef enum
{
    PM_RANGE_UNINITIALIZED = 0, // intentionally 0
    PM_RANGE_FREE = 1,
    PM_RANGE_RESERVED = 2,
    PM_RANGE_ALLOCATED = 3,
} pm_range_type_t;

// represents a physical frame, there will be one `phyframe_t` for each physical frame in the system
typedef struct phyframe
{
    as_linked_list; // freelist or allocated list
    size_t order;   // order of the frame (2^order pages)

    enum phyframe_state
    {
        PHYFRAME_RESERVED = 0, // intentionally 0
        PHYFRAME_FREE,
        PHYFRAME_ALLOCATED,
    } state;

    union
    {
        struct // mapped frame
        {
            // number of times this frame is mapped
            // if this drops to 0, the frame is freed
            atomic_t mapped_count;
        };
    };
} phyframe_t;

typedef struct
{
    pfn_t pfn_start;
    size_t nframes;
    bool reserved;
} pmm_region_t;

/**
 * @brief Dump the physical memory manager's state, (i.e. the free list and the allocated list).
 */
void pmm_dump_lists(void);

/**
 * @brief Initialize the physical memory manager.
 *
 * @param max_frames Maximum number of frames that are addressable on the system.
 */
void pmm_init(size_t max_frames);

/**
 * @brief Allocate n_frames of contiguous physical memory.
 *
 * @param n_frames Number of frames to allocate.
 * @return pfn_t The physical frame number of the first frame in the contiguous block.
 *
 * @note
 */
pfn_t pmm_allocate_frames(size_t n_frames);

/**
 * @brief Increase the reference count of a list of blocks of physical memory.
 *
 * @param pfn_start Physical frame number of the block to reference.
 * @param npages Number of pages to reference.
 *
 * @note Only allocated blocks can be referenced, if one wants to reference a reserved block,
 * use \ref pmm_reserve_frames or \ref pmm_reserve_block first to reserve the block, which only
 * needs to be done once.
 *
 */
void pmm_ref_frames(pfn_t pfn_start, size_t npages);

/**
 * @brief Unreference a list of blocks of physical memory.
 *
 * @param pfn_start Physical frame number of the block to unreference.
 * @param npages Number of pages to unreference.
 *
 * @note If the reference count of the block reaches 0, the block will be freed and ready to
 * be re-allocated in the future.
 *
 */
void pmm_unref_frames(pfn_t pfn_start, size_t npages);

/**
 * @brief Add a region of physical memory to the physical memory manager.
 *
 * @param start
 * @param nframes
 * @param reserved
 */
void pmm_register_region(pfn_t start, size_t nframes, bool reserved);

/**
 * @brief Mark a range of physical memory as reserved.
 *
 * @param paddr Physical address of the block to reserve.
 * @param npages Number of pages to reserve.
 * @return pfn_t Physical frame number of the first frame in the reserved block.
 *
 * @note The memory will be marked as `PMM_REGION_RESERVED` and will be moved to the allocated list.
 *
 * @note If the range does not exist in the free list and overlaps with an allocated block,
 * the kernel panics.
 *
 */
pfn_t pmm_reserve_frames(pfn_t pfn, size_t npages);
#define pmm_reserve_address(paddr)           pmm_reserve_frames(ALIGN_DOWN_TO_PAGE(paddr) / MOS_PAGE_SIZE, 1)
#define pmm_reserve_addresses(paddr, npages) pmm_reserve_frames(ALIGN_DOWN_TO_PAGE(paddr) / MOS_PAGE_SIZE, npages)

/**
 * @brief Find a region in the physical memory manager.
 *
 * @param needle One address in the region to find.
 * @return pmm_region_t* The region found, or NULL if not found.
 */
pmm_region_t *pmm_find_reserved_region(ptr_t needle);

/** @} */
