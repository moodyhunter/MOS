// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mm/paging/paging.h>
#include <mos/tasks/task_types.h>
#include <mos/types.h>

/**
 * @brief Map a page into the current process's address space
 *
 * @param hint_addr A hint for the address to map the page at, the actual address is determined based on the @p flags
 * @param flags Flags to control the mapping, see @ref mmap_flags_t
 * @param vm_flags Flags to control the permissions of the mapping, see @ref vm_flags
 * @param n_pages Number of pages to map
 * @return ptr_t The address the page was mapped at, or 0 if the mapping failed
 */
ptr_t mmap_anonymous(ptr_t hint_addr, mmap_flags_t flags, vm_flags vm_flags, size_t n_pages);

/**
 * @brief Map a file into the current process's address space
 *
 * @param hint_addr A hint for the address to map the page at, the actual address is determined based on the @p flags
 * @param flags Flags to control the mapping, see @ref mmap_flags_t
 * @param vm_flags Flags to control the permissions of the mapping, see @ref vm_flags
 * @param n_pages Number of pages to map
 * @param io The io object to map, the io object must be backed by a @ref file_t
 * @param offset The offset into the file to map, must be page aligned
 * @return ptr_t The address the page was mapped at, or 0 if the mapping failed
 */
ptr_t mmap_file(ptr_t hint_addr, mmap_flags_t flags, vm_flags vm_flags, size_t n_pages, io_t *io, off_t offset);

/**
 * @brief Unmap a page from the current process's address space
 *
 * @param addr The address to unmap
 * @param size The size of the mapping to unmap
 * @return true If the unmap succeeded
 *
 * @note Neither @p addr nor @p size need to be page aligned, all pages contained within
 * the range specified will be unmapped even if they are not fully contained within the range.
 */
bool munmap(ptr_t addr, size_t size);
