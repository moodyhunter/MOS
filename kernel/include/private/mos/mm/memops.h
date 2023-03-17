// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"

vmblock_t mm_alloc_zeroed_pages(paging_handle_t handle, size_t npages, pgalloc_hints hints, vm_flags flags);
vmblock_t mm_alloc_zeroed_pages_at(paging_handle_t handle, uintptr_t vaddr, size_t npages, vm_flags flags);
