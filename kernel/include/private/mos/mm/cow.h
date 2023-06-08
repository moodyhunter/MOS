// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mm/paging/paging.h>
#include <mos/platform/platform.h>

/**
 * @brief Initialize copy-on-write
 *
 */
void mm_cow_init(void);

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
vmblock_t mm_make_cow(mm_context_t *from, ptr_t fvaddr, mm_context_t *to, ptr_t tvaddr, size_t npages, vm_flags flags);
vmblock_t mm_make_cow_block(mm_context_t *target_handle, vmblock_t src_block);

/**
 * @brief Handle a page fault
 *
 * @param fault_addr The fault address
 * @param present Whether the page is present
 * @param is_write Whether the fault is caused by a write
 * @param is_user Whether the fault is caused by a user process
 * @param is_exec Whether the fault is caused by an instruction fetch
 * @return true If the fault is handled
 */
bool mm_handle_pgfault(ptr_t fault_addr, bool present, bool is_write, bool is_user, bool is_exec);

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
vmblock_t mm_alloc_zeroed_pages(mm_context_t *handle, size_t npages, ptr_t vaddr, valloc_flags hints, vm_flags flags);
