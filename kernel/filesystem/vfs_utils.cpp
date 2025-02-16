// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/vfs_utils.hpp"

#include "mos/filesystem/dentry.hpp"
#include "mos/filesystem/page_cache.hpp"
#include "mos/filesystem/vfs_types.hpp"
#include "mos/lib/sync/spinlock.hpp"
#include "mos/mm/physical/pmm.hpp"

#include <algorithm>
#include <memory>
#include <mos/lib/structures/hashmap_common.hpp>
#include <mos/types.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

static dentry_t *dentry_create(superblock_t *sb, dentry_t *parent, mos::string_view name)
{
    std::unique_ptr<int> a;
    const auto dentry = mos::create<dentry_t>();
    tree_node_init(tree_node(dentry));

    dentry->superblock = sb;
    dentry->name = name;

    if (parent)
    {
        MOS_ASSERT(spinlock_is_locked(&parent->lock));
        tree_add_child(tree_node(parent), tree_node(dentry));
        dentry->superblock = parent->superblock;
    }

    return dentry;
}

dentry_t *dentry_get_from_parent(superblock_t *sb, dentry_t *parent, mos::string_view name)
{
    if (!parent)
        return dentry_create(sb, NULL, name);

    dentry_t *dentry = NULL;

    spinlock_acquire(&parent->lock);
    tree_foreach_child(dentry_t, child, parent)
    {
        if (child->name == name)
        {
            dentry = child;
            break;
        }
    }

    // if not found, create a new one
    if (!dentry)
        dentry = dentry_create(sb, parent, name);

    spinlock_release(&parent->lock);
    return dentry;
}

bool simple_page_write_begin(inode_cache_t *icache, off_t offset, size_t size, phyframe_t **page, void **private_)
{
    MOS_UNUSED(size);
    const auto newPage = pagecache_get_page_for_write(icache, offset / MOS_PAGE_SIZE);
    if (newPage.isErr())
        return false;

    *page = newPage.get();
    *private_ = NULL;
    return true;
}

void simple_page_write_end(inode_cache_t *icache, off_t offset, size_t size, phyframe_t *page, void *private_)
{
    MOS_UNUSED(page);
    MOS_UNUSED(private_);

    // also update the inode's size
    if (offset + size > icache->owner->size)
        icache->owner->size = offset + size;
}

long simple_flush_page_discard_data(inode_cache_t *icache, off_t pgoff, phyframe_t *page)
{
    MOS_UNUSED(icache);
    MOS_UNUSED(pgoff);
    MOS_UNUSED(page);
    return 0;
}

// read from the page cache, the size and offset are already validated to be in the file's bounds
ssize_t vfs_generic_read(const file_t *file, void *buf, size_t size, off_t offset)
{
    // cap the read size to the file's size
    size = std::min(size, file->dentry->inode->size - offset);
    inode_cache_t *icache = &file->dentry->inode->cache;
    const ssize_t read = vfs_read_pagecache(icache, buf, size, offset);
    return read;
}

// write to the page cache, the size and offset are already validated to be in the file's bounds
ssize_t vfs_generic_write(const file_t *file, const void *buf, size_t size, off_t offset)
{
    inode_cache_t *icache = &file->dentry->inode->cache;
    const ssize_t written = vfs_write_pagecache(icache, buf, size, offset);
    return written;
}

bool vfs_simple_write_begin(inode_cache_t *icache, off_t offset, size_t size)
{
    MOS_UNUSED(icache);
    MOS_UNUSED(offset);
    MOS_UNUSED(size);
    return true;
}

void vfs_generic_iterate_dir(const dentry_t *dir, vfs_listdir_state_t *state, dentry_iterator_op add_record)
{
    dentry_t *d_parent = dentry_parent(*dir);
    if (d_parent == NULL)
        d_parent = root_dentry;

    MOS_ASSERT(d_parent->inode != NULL);
    MOS_ASSERT(dir->inode);

    add_record(state, dir->inode->ino, ".", FILE_TYPE_DIRECTORY);
    add_record(state, d_parent->inode->ino, "..", FILE_TYPE_DIRECTORY);

    tree_foreach_child(dentry_t, child, dir)
    {
        if (child->inode)
            add_record(state, child->inode->ino, child->name, child->inode->type);
    }
}
