// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mm/paging/paging.h>
#include <mos/platform/platform.h>

/**
 * @brief Allocate zero-on-demand pages at a specific address
 *
 * @param handle The paging handle to use
 * @param npages The number of pages to allocate
 * @param vaddr The virtual address to allocate at
 * @param hints Allocation hints, see valloc_flags
 * @param flags VM flags to use
 * @return vmblock_t The allocated block
 */
vmblock_t mm_alloc_zeroed_pages(paging_handle_t handle, size_t npages, uintptr_t vaddr, valloc_flags hints, vm_flags flags);
