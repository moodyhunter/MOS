// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/mm/mm.h"
#include "mos/mm/paging/paging.h"
#include "mos/tasks/task_types.h"

/**
 * @brief Make a copy-on-write block
 *
 * @param from Source paging handle
 * @param fvaddr Source virtual address
 * @param to Target paging handle
 * @param tvaddr Target virtual address
 * @param npages Number of pages to copy
 * @param flags VM flags to use, VM_WRITE will be ignored
 * @return vmblock_t The allocated block, but with original flags set
 */
vmap_t *cow_clone_vmap_locked(mm_context_t *target_mmctx, vmap_t *source_vmap);

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
vmap_t *cow_allocate_zeroed_pages(mm_context_t *handle, size_t npages, ptr_t vaddr, valloc_flags hints, vm_flags flags);
