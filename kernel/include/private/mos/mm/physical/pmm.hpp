// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/lib/structures/list.hpp>
#include <mos/mos_global.h>
#include <mos/types.hpp>

/**
 * @defgroup pmm Physical Memory Manager
 * @ingroup mm
 * @brief Allocate/free the physical memory, in the form of physical frames.
 *
 * @details
 * The physical memory manager is responsible for allocating and freeing physical memory.
 *
 * @note The allocated physical memory is not zeroed out, in fact, it does nothing to the real
 * memory content except for marking it as allocated.
 *
 * @{
 */

typedef struct phyframe phyframe_t;

// represents a physical frame, there will be one `phyframe_t` for each physical frame in the system
typedef struct phyframe
{
    enum phyframe_state
    {
        PHYFRAME_RESERVED = 0, // intentionally 0 (initially, all frames are reserved)
        PHYFRAME_FREE,
        PHYFRAME_ALLOCATED,
    } state;

    u8 order;

    union additional_info
    {
        MOS_WARNING_PUSH
        MOS_WARNING_DISABLE("-Wpedantic") // ISO C++ forbids anonymous structs

        list_node_t list_node; // for use of freelist in the buddy allocator

        struct pagecache_frame_info // allocated frame
        {
            // for page cache frames
            bool dirty : 1; ///< 1 if the page is dirty
        } pagecache;
        MOS_WARNING_POP
    } info;

    union alloc_info
    {
        // number of times this frame is mapped, if this drops to 0, the frame is freed
        atomic_t refcount;
    } alloc;
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
} pmm_allocation_flags_t;

extern phyframe_t *phyframes; // array of all physical frames

#define phyframe_pfn(frame) ((pfn_t) ((frame) - phyframes))
#define pfn_phyframe(pfn)   (&phyframes[(pfn)])

extern size_t pmm_total_frames, pmm_allocated_frames, pmm_reserved_frames;

/**
 * @brief Dump the physical memory manager's state, (i.e. the free list and the allocated list).
 */
void pmm_dump_lists(void);

void pmm_init();

/**
 * @brief Allocate n_frames of contiguous physical memory.
 *
 * @param n_frames Number of frames to allocate.
 * @param flags Allocation flags.
 * @return phyframe_t* Pointer to the first frame of the allocated block.
 */
phyframe_t *pmm_allocate_frames(size_t n_frames, pmm_allocation_flags_t flags);
void pmm_free_frames(phyframe_t *start_frame, size_t n_pages);

/**
 * @brief Mark a range of physical memory as reserved.
 *
 * @param pfn Physical frame number of the block to reserve.
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

// ! ref and unref frames

phyframe_t *_pmm_ref_phyframes(phyframe_t *frame, size_t npages);
void _pmm_unref_phyframes(phyframe_t *frame, size_t npages);

should_inline pfn_t _pmm_ref_pfn_range(pfn_t pfn_start, size_t npages)
{
    _pmm_ref_phyframes(pfn_phyframe(pfn_start), npages);
    return pfn_start;
}

should_inline void _pmm_unref_pfn_range(pfn_t pfn_start, size_t npages)
{
    _pmm_unref_phyframes(pfn_phyframe(pfn_start), npages);
}

#ifdef __cplusplus
should_inline phyframe_t *pmm_ref(phyframe_t *frame, size_t npages = 1)
{
    return _pmm_ref_phyframes(frame, npages);
}
should_inline pfn_t pmm_ref(pfn_t pfn_start, size_t npages = 1)
{
    return _pmm_ref_pfn_range(pfn_start, npages);
}

should_inline void pmm_unref(phyframe_t *frame, size_t npages = 1)
{
    _pmm_unref_phyframes(frame, npages);
}
should_inline void pmm_unref(pfn_t pfn_start, size_t npages = 1)
{
    _pmm_unref_pfn_range(pfn_start, npages);
}
#else
#define pmm_ref(thing, npages)   _Generic((thing), phyframe_t *: _pmm_ref_phyframes, pfn_t: _pmm_ref_pfn_range)(thing, npages)
#define pmm_unref(thing, npages) _Generic((thing), phyframe_t *: _pmm_unref_phyframes, pfn_t: _pmm_unref_pfn_range)(thing, npages)
#endif

#define pmm_ref_one(thing)   pmm_ref(thing, 1)
#define pmm_unref_one(thing) pmm_unref(thing, 1)

/** @} */
