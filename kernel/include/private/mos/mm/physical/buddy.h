// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/physical/pmm.h"

#include <mos/types.h>

#define MOS_INVALID_PFN ((pfn_t) -1)

extern size_t buddy_max_nframes;
extern phyframe_t *phyframes; // array of all physical frames

/**
 * @brief Dump the state of the buddy allocator.
 *
 */
void phyframe_dump_all();

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
 * @return pfn_t The physical frame number of the first frame in the contiguous block.
 */
pfn_t buddy_alloc_n(size_t nframes);

/**
 * @brief Free nframes of contiguous physical memory.
 *
 * @param pfn The physical frame number of the first frame in the contiguous block.
 * @param nframes The number of frames to free.
 *
 * @note // !! The number of frames freed must be the same as the number of frames allocated. // TODO
 */
void buddy_free_n(pfn_t pfn, size_t nframes);
