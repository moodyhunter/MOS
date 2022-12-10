// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"
#include "mos/tasks/task_types.h"
#include "mos/types.h"

void shm_init(void);
vmblock_t shm_allocate(process_t *owner, size_t npages, mmap_flags flags);
