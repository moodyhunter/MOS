// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/physical/pmm.h"

#include <mos/types.h>

/**
 * @brief Dump the state of the buddy allocator.
 *
 */
void buddy_dump_all();

/**
 * @brief Initialize the buddy allocator with the given maximum number of frames.
 *
 * @param max_nframes The maximum number of frames that are addressable on the system.
 */
void buddy_init(size_t max_nframes);

/**
 * @brief Reserve several frames at the given physical frame number.
 *
 * @param pfn The physical frame number to reserve.
 * @param nframes The number of frames to reserve.
 */
void buddy_reserve_n(pfn_t pfn, size_t nframes);

/**
 * @brief Allocate nframes of contiguous physical memory.
 *
 * @param nframes The number of frames to allocate.
 * @return phyframe_t * The first frame in the contiguous block.
 *
 * @note nframes will be allocated exactly, and each frame can be freed individually.
 *       (unlike buddy_alloc_n_compound)
 */
phyframe_t *buddy_alloc_n_exact(size_t nframes);

/**
 * @brief Allocate nframes of contiguous physical memory.
 *
 * @param nframes The number of frames to allocate.
 * @return phyframe_t * The first frame in the contiguous block.
 *
 * @note nframes will be rounded up to the nearest power of 2, and the entire compound page must be freed at once.
 *       users can use the mapped_count field of the phyframe_t to keep track of the number of frames that are still
 *       in use, and free the compound page when the count drops to 0.
 */
phyframe_t *buddy_alloc_n_compound(size_t nframes);

/**
 * @brief Free nframes of contiguous physical memory.
 *
 * @param pfn The physical frame number of the first frame in the contiguous block.
 * @param nframes The number of frames to free.
 *
 * @note // !! The number of frames freed must be the same as the number of frames allocated. // TODO
 */
void buddy_free_n(pfn_t pfn, size_t nframes);
