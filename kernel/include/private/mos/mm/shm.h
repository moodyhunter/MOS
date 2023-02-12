// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mm_types.h"
#include "mos/tasks/task_types.h"

void shm_init(void);
shm_block_t shm_allocate(size_t npages, mmap_flags flags, vm_flags vmflags);
vmblock_t shm_map_shared_block(shm_block_t source);
