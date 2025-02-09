// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mm.hpp"
#include "mos/mm/paging/paging.hpp"

/**
 * @brief Copy-on-write a page range
 *
 * @param target_mmctx The mm context to copy to
 * @param source_vmap The vmap to copy from
 * @return vmap_t* The new vmap, with the lock held
 */
PtrResult<vmap_t> cow_clone_vmap_locked(mm_context_t *target_mmctx, vmap_t *source_vmap);

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
PtrResult<vmap_t> cow_allocate_zeroed_pages(mm_context_t *handle, size_t npages, ptr_t vaddr, valloc_flags hints, vm_flags flags);
