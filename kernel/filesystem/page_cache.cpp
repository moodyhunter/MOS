// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/page_cache.hpp"

#include "mos/filesystem/vfs_types.hpp"
#include "mos/filesystem/vfs_utils.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/mmstat.hpp"
#include "mos/mm/physical/pmm.hpp"
#include "mos/syslog/printk.hpp"

#include <algorithm>
#include <mos/mos_global.h>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

struct _flush_and_drop_data
{
    inode_cache_t *icache;
    bool should_drop_page;
    long ret;
};

static bool do_flush_and_drop_cached_page(const uintn key, void *value, void *data)
{
    // key = page number, value = phyframe_t *, data = _flush_and_drop_data *
    struct _flush_and_drop_data *const fdd = static_cast<struct _flush_and_drop_data *>(data);
    inode_cache_t *const icache = fdd->icache;
    phyframe_t *const page = static_cast<phyframe_t *>(value);
    off_t const pgoff = key;
    const bool drop_page = fdd->should_drop_page;

    // !! TODO
    // !! currently we have no reliable way to track if a page is dirty
    // !! so we always flush it
    // !! this causes performance issues, but it's simple and works for now
    // if (!page->pagecache.dirty)

    long ret = 0;
    if (icache->ops->flush_page)
        ret = icache->ops->flush_page(icache, pgoff, page);
    else
        ret = simple_flush_page_discard_data(icache, pgoff, page);

    if (!IS_ERR_VALUE(ret) && drop_page)
    {
        // only when the page was successfully flushed
        hashmap_remove(&icache->pages, pgoff);
        mmstat_dec1(MEM_PAGECACHE);
        pmm_unref_one(page);
    }

    fdd->ret = ret;
    return ret == 0;
}

long pagecache_flush_or_drop(inode_cache_t *icache, off_t pgoff, size_t npages, bool drop_page)
{
    struct _flush_and_drop_data data = { .icache = icache, .should_drop_page = drop_page, .ret = 0 };

    long ret = 0;
    for (size_t i = 0; i < npages; i++)
    {
        phyframe_t *page = (phyframe_t *) hashmap_get(&icache->pages, pgoff + i);
        if (!page)
            continue;

        do_flush_and_drop_cached_page(pgoff + i, page, &data);

        if (data.ret != 0)
        {
            ret = data.ret;
            break;
        }
    }
    return ret;
}

long pagecache_flush_or_drop_all(inode_cache_t *icache, bool drop_page)
{
    struct _flush_and_drop_data data = { .icache = icache, .should_drop_page = drop_page, .ret = 0 };
    hashmap_foreach(&icache->pages, do_flush_and_drop_cached_page, &data);
    return data.ret;
}

PtrResult<phyframe_t> pagecache_get_page_for_read(inode_cache_t *cache, off_t pgoff)
{
    {
        const auto page = static_cast<phyframe_t *>(hashmap_get(&cache->pages, pgoff));
        if (page)
            return page; // fast path
    }

    if (!cache->ops)
        return -EIO;

    MOS_ASSERT_X(cache->ops && cache->ops->fill_cache, "no page cache ops for inode %p", (void *) cache->owner);
    const auto newPage = cache->ops->fill_cache(cache, pgoff);
    if (newPage.isErr())
        return newPage;

    mmstat_inc1(MEM_PAGECACHE);
    const auto prev = hashmap_put(&cache->pages, pgoff, static_cast<void *>(newPage.get()));
    MOS_ASSERT(prev == nullptr);
    return newPage;
}

PtrResult<phyframe_t> pagecache_get_page_for_write(inode_cache_t *cache, off_t pgoff)
{
    return pagecache_get_page_for_read(cache, pgoff);
}

ssize_t vfs_read_pagecache(inode_cache_t *icache, void *buf, size_t size, off_t offset)
{
    mutex_acquire(&icache->lock);
    size_t bytes_read = 0;
    size_t bytes_left = size;
    while (bytes_left > 0)
    {
        // bytes to copy from the current page
        const size_t inpage_offset = offset % MOS_PAGE_SIZE;
        const size_t inpage_size = std::min(MOS_PAGE_SIZE - inpage_offset, bytes_left); // in case we're at the end of the file,

        auto page = pagecache_get_page_for_read(icache, offset / MOS_PAGE_SIZE); // the initial page
        if (page.isErr())
        {
            mutex_release(&icache->lock);
            return page.getErr();
        }

        memcpy((char *) buf + bytes_read, (void *) (phyframe_va(page.get()) + inpage_offset), inpage_size);

        bytes_read += inpage_size;
        bytes_left -= inpage_size;
        offset += inpage_size;
    }

    mutex_release(&icache->lock);
    return bytes_read;
}

ssize_t vfs_write_pagecache(inode_cache_t *icache, const void *buf, size_t total_size, off_t offset)
{
    const inode_cache_ops_t *ops = icache->ops;
    MOS_ASSERT_X(ops, "no page cache ops for inode %p", (void *) icache->owner);

    mutex_acquire(&icache->lock);

    size_t bytes_written = 0;
    size_t bytes_left = total_size;
    while (bytes_left > 0)
    {
        // bytes to copy to the current page
        const size_t inpage_offset = offset % MOS_PAGE_SIZE;
        const size_t inpage_size = std::min(MOS_PAGE_SIZE - inpage_offset, bytes_left); // in case we're at the end of the file,

        void *private_data;
        phyframe_t *page;
        const bool can_write = ops->page_write_begin(icache, offset, inpage_size, &page, &private_data);
        if (!can_write)
        {
            pr_warn("page_write_begin failed");
            mutex_release(&icache->lock);
            return -EIO;
        }

        memcpy((char *) (phyframe_va(page) + inpage_offset), (char *) buf + bytes_written, inpage_size);
        ops->page_write_end(icache, offset, inpage_size, page, private_data);

        bytes_written += inpage_size;
        bytes_left -= inpage_size;
        offset += inpage_size;
    }

    mutex_release(&icache->lock);
    return bytes_written;
}
