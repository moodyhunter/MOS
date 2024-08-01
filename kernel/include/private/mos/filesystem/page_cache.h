// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.h"
#include "mos/mm/physical/pmm.h"

/**
 * @brief Get a page from the page cache
 *
 * @param cache The inode cache
 * @param pgoff The page offset
 * @return phyframe_t* The page, or NULL if it doesn't exist
 * @note Caller must hold the cache's lock
 */
phyframe_t *pagecache_get_page_for_read(inode_cache_t *cache, off_t pgoff);

/**
 * @brief Get a page from the page cache for writing
 *
 * @param cache The inode cache
 * @param pgoff The page offset
 * @return phyframe_t* The page
 */
phyframe_t *pagecache_get_page_for_write(inode_cache_t *cache, off_t pgoff);

ssize_t vfs_read_pagecache(inode_cache_t *icache, void *buf, size_t size, off_t offset);
ssize_t vfs_write_pagecache(inode_cache_t *icache, const void *buf, size_t total_size, off_t offset);

/**
 * @brief Flush or drop a range of pages from the page cache
 *
 * @param icache The inode cache
 * @param pgoff The starting page offset
 * @param npages The number of pages to flush or drop
 * @param drop_page Whether to drop the pages, or just flush them
 * @return long
 */
long pagecache_flush_or_drop(inode_cache_t *icache, off_t pgoff, size_t npages, bool drop_page);

/**
 * @brief Flush or drop all pages in the page cache
 *
 * @param icache The inode cache
 * @param drop_page Whether to drop the pages, or just flush them
 * @return long
 */
long pagecache_flush_or_drop_all(inode_cache_t *icache, bool drop_page);
