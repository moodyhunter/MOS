// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mm.h"

#include <mos/tasks/task_types.h>

/**
 * @defgroup mm Memory Management
 * @brief Memory management subsystem
 * @{
 */

/**
 * @brief Allocate a new shared memory block
 *
 * @param npages The number of pages to allocate
 * @param mode The fork mode to use
 * @param vmflags The VM flags to use
 * @return vmblock_t The allocated block
 *
 * @note Internally a block of shared memory has no difference from a block of
 * memory that is not shared, this is just a convenience wrapper around
 * mm_alloc_pages.
 */
vmap_t *shm_allocate(size_t npages, vmap_fork_behavior_t mode, vm_flags vmflags);

/**
 * @brief Map a shared memory block
 *
 * @param source The block to map
 * @param mode The fork mode to use
 * @return vmblock_t The mapped block
 *
 * @note Internally a block of shared memory has no difference from a block of
 * memory that is not shared, this is just a convenience wrapper around
 * mm_copy_mapping.
 */
vmap_t *shm_map_shared_block(vmap_t *source, vmap_fork_behavior_t mode);

/** @} */
