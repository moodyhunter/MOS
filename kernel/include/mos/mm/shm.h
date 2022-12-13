// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mm_types.h"
#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"
#include "mos/types.h"

#include MOS_KERNEL_INTERNAL_HEADER_CHECK

void shm_init(void);
shm_block_t shm_allocate(process_t *owner, size_t npages, mmap_flags flags, vm_flags vmflags);
vmblock_t shm_map_shared_block(shm_block_t source, process_t *owner);
