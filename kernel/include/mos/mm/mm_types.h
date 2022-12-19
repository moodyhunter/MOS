// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/types.h"

/**
 * @defgroup mm Memory Management
 * @brief Memory management subsystem
 * @{
 */

typedef struct
{
    paging_handle_t address_space;
    vmblock_t block;
} shm_block_t;

/** @} */
