// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/platform/platform.h"

/**
 * @brief Allocate zero-on-demand pages
 *
 * @param handle The paging handle to use
 * @param npages The number of pages to allocate
 * @param hints Allocation hints, see pgalloc_hints
 * @param flags VM flags to use
 * @return vmblock_t The allocated block
 */
vmblock_t mm_alloc_zeroed_pages(paging_handle_t handle, size_t npages, pgalloc_hints hints, vm_flags flags);

/**
 * @brief Allocate zero-on-demand pages at a specific address
 *
 * @param handle The paging handle to use
 * @param vaddr The virtual address to allocate at
 * @param npages The number of pages to allocate
 * @param flags VM flags to use
 * @return vmblock_t The allocated block
 */
vmblock_t mm_alloc_zeroed_pages_at(paging_handle_t handle, uintptr_t vaddr, size_t npages, vm_flags flags);
