// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mm_types.h"
#include "mos/tasks/task_types.h"

/**
 * @defgroup mm Memory Management
 * @brief Memory management subsystem
 * @{
 */

void shm_init(void);
vmblock_t shm_allocate(size_t npages, vmap_fork_mode_t mode, vm_flags vmflags);
vmblock_t shm_map_shared_block(vmblock_t source, vmap_fork_mode_t mode);

/** @} */
