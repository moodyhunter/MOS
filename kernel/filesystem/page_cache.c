// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/page_cache.h"

#include "mos/lib/sync/spinlock.h"
#include "mos/mm/mm.h"
#include "mos/mm/mmstat.h"
#include "mos/mm/physical/pmm.h"
#include "mos/syslog/printk.h"

#include <mos_stdlib.h>
#include <mos_string.h>

phyframe_t *pagecache_get_page_for_read(inode_cache_t *cache, off_t pgoff)
{
    MOS_ASSERT(spinlock_is_locked(&cache->lock));

    void *p = hashmap_get(&cache->pages, pgoff);
    if (p)
        return p;

    MOS_ASSERT_X(cache->ops && cache->ops->fill_cache, "no page cache ops for inode %p", (void *) cache->owner);
    phyframe_t *page = cache->ops->fill_cache(cache, pgoff);
    if (IS_ERR(page))
        return page;
    mmstat_inc1(MEM_PAGECACHE);
    MOS_ASSERT(hashmap_put(&cache->pages, pgoff, page) == NULL);
    return page;
}

phyframe_t *pagecache_get_page_for_write(inode_cache_t *cache, off_t pgoff)
{
    return pagecache_get_page_for_read(cache, pgoff);
}

ssize_t vfs_read_pagecache(inode_cache_t *icache, void *buf, size_t size, off_t offset)
{
    spinlock_acquire(&icache->lock);
    size_t bytes_read = 0;
    size_t bytes_left = size;
    while (bytes_left > 0)
    {
        // bytes to copy from the current page
        const size_t inpage_offset = offset % MOS_PAGE_SIZE;
        const size_t inpage_size = MIN(MOS_PAGE_SIZE - inpage_offset, bytes_left); // in case we're at the end of the file,

        phyframe_t *page = pagecache_get_page_for_read(icache, offset / MOS_PAGE_SIZE); // the initial page
        if (IS_ERR(page))
        {
            spinlock_release(&icache->lock);
            return PTR_ERR(page);
        }

        memcpy((char *) buf + bytes_read, (void *) (phyframe_va(page) + inpage_offset), inpage_size);

        bytes_read += inpage_size;
        bytes_left -= inpage_size;
        offset += inpage_size;
    }

    spinlock_release(&icache->lock);
    return bytes_read;
}

ssize_t vfs_write_pagecache(inode_cache_t *icache, const void *buf, size_t total_size, off_t offset)
{
    const inode_cache_ops_t *ops = icache->ops;
    MOS_ASSERT_X(ops, "no page cache ops for inode %p", (void *) icache->owner);

    spinlock_acquire(&icache->lock);

    size_t bytes_written = 0;
    size_t bytes_left = total_size;
    while (bytes_left > 0)
    {
        // bytes to copy to the current page
        const size_t inpage_offset = offset % MOS_PAGE_SIZE;
        const size_t inpage_size = MIN(MOS_PAGE_SIZE - inpage_offset, bytes_left); // in case we're at the end of the file,

        void *private;
        phyframe_t *page;
        const bool can_write = ops->page_write_begin(icache, offset, inpage_size, &page, &private);
        if (!can_write)
        {
            pr_warn("page_write_begin failed");
            spinlock_release(&icache->lock);
            return -EIO;
        }

        memcpy((char *) (phyframe_va(page) + inpage_offset), (char *) buf + bytes_written, inpage_size);
        ops->page_write_end(icache, offset, inpage_size, page, private);

        bytes_written += inpage_size;
        bytes_left -= inpage_size;
        offset += inpage_size;
    }

    spinlock_release(&icache->lock);
    return bytes_written;
}
