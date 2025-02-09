// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/types.hpp>

/**
 * @brief Allocate DMA pages
 *
 * @param n_pages Number of pages to allocate
 * @param pages Pointer to store the virtual address of the pages
 * @return physical frame number of the starting page
 */
pfn_t dmabuf_allocate(size_t n_pages, ptr_t *pages);

bool dmabuf_free(ptr_t vaddr, ptr_t paddr);

pfn_t dmabuf_share(void *buffer, size_t size);

bool dmabuf_unshare(ptr_t phys, size_t size, void *virt);
