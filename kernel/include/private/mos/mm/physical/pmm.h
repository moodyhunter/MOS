// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/lib/sync/spinlock.h"
#include "mos/platform/platform_defs.h"

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

typedef struct phyframe phyframe_t;

// represents a physical frame, there will be one `phyframe_t` for each physical frame in the system
typedef struct phyframe
{
    enum phyframe_state
    {
        PHYFRAME_RESERVED = 0, // intentionally 0
        PHYFRAME_FREE,
        PHYFRAME_ALLOCATED,
    } state;

    u8 order;

    union
    {
        struct // free frame
        {
            as_linked_list;
        };

        union // compound frame
        {
            // whether this frame is the tail of a compound page
            bool compound_tail;
            phyframe_t *compound_head; // compound head
        };
    };

    union
    {
        // number of times this frame is mapped, if this drops to 0, the frame is freed
        atomic_t refcount;
    };

} phyframe_t;

MOS_STATIC_ASSERT(sizeof(phyframe_t) == 32, "update phyframe_t struct size");

typedef struct
{
    pfn_t pfn_start;
    size_t nframes;
    bool reserved;
    u32 type; // platform-specific type
} pmm_region_t;

typedef enum
{
    /// allocate normal pages
    PMM_ALLOC_NORMAL = 0,
    /// allocate compound pages when npages > 1
    PMM_ALLOC_COMPOUND = 1 << 0,
} pmm_allocation_flags_t;

#define MOS_INVALID_PFN  ((pfn_t) -1)
#define pfn_invalid(pfn) ((pfn) == MOS_INVALID_PFN)

extern phyframe_t *phyframes; // array of all physical frames
extern size_t buddy_max_nframes;

static inline __maybe_unused pfn_t phyframe_pfn(const phyframe_t *frame)
{
    return frame - phyframes;
}

static inline __maybe_unused phyframe_t *phyframe_effective_head(phyframe_t *frame)
{
    MOS_ASSERT_X(frame->state == PHYFRAME_ALLOCATED || frame->state == PHYFRAME_RESERVED, "WRONG");
    MOS_ASSERT(frame->compound_tail == 0 || frame->compound_tail == 1);
    return frame->compound_tail == true ? frame->compound_head : frame;
}

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
 * @return phyframe_t* Pointer to the first frame of the allocated block.
 *
 * @note
 */
phyframe_t *pmm_allocate_frames(size_t n_frames, pmm_allocation_flags_t flags);

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
 * @brief Reference a page of physical memory.
 *
 * @param frame
 */
phyframe_t *pmm_ref_frame(phyframe_t *frame);

/**
 * @brief Unreference a page of physical memory.
 *
 * @param frame
 */
void pmm_unref_frame(phyframe_t *frame);

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
