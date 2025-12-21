// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/vfs_utils.hpp"

#include "mos/filesystem/page_cache.hpp"
#include "mos/filesystem/vfs_types.hpp"
#include "mos/lib/sync/spinlock.hpp"
#include "mos/mm/physical/pmm.hpp"
#include "mos/syslog/debug.hpp"

#include <algorithm>
#include <mos/types.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

ptr<dentry_t> dentry_create(superblock_t *sb, ptr<dentry_t> parent, mos::string_view name)
{
    MOS_ASSERT_X(parent && !name.empty(), "Cannot create non-root dentry with NULL parent");
    const auto child = mos::make_shared<dentry_t>();
    dInfo2<dcache> << fmt("allocated new dentry '{}' at {}, parent '{}'", name, (void *) child.get(), (void *) parent.get());

    child->superblock = sb;
    child->name = name;

    dInfo2<dcache> << fmt("adding dentry '{}' to parent {}", name, (void *) parent.get());
    MOS_ASSERT(spinlock_is_locked(&parent->lock));
    parent->children.push_back(child);
    child->superblock = parent->superblock;
    child->parent = parent;

    return child;
}

ptr<dentry_t> dentry_create_root(superblock_t *sb)
{
    const auto child = mos::make_shared<dentry_t>();
    dInfo2<dcache> << fmt("allocated new root dentry at {}", (void *) child.get());
    child->superblock = sb;
    return child;
}

ptr<dentry_t> dentry_get_or_create_child(ptr<dentry_t> const parent, superblock_t *sb, mos::string_view name)
{
    MOS_ASSERT_X(parent != nullptr, "Parent dentry cannot be null");
    dInfo<dcache> << fmt("looking up child '{}' in parent {}", name, (void *) parent.get());

    SpinLocker locker(&parent->lock);
    for (const auto &child : parent->children)
    {
        if (child->name == name)
        {
            dInfo2<dcache> << fmt("  found existing dentry '{}' at {}", name, (void *) child.get());
            return child;
        }
    }

    // if not found, create a new one
    ptr<dentry_t> const dentry = dentry_create(sb, parent, name);
    dEmph<dcache> << fmt("  created new dentry '{}' at {}", name, (void *) dentry.get());
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
ssize_t vfs_generic_read(const FsBaseFile *file, void *buf, size_t size, off_t offset)
{
    // cap the read size to the file's size
    size = std::min(size, file->dentry->inode->size - offset);
    inode_cache_t *icache = &file->dentry->inode->cache;
    const ssize_t read = vfs_read_pagecache(icache, buf, size, offset);
    return read;
}

// write to the page cache, the size and offset are already validated to be in the file's bounds
ssize_t vfs_generic_write(const FsBaseFile *file, const void *buf, size_t size, off_t offset)
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

void vfs_generic_iterate_dir(const ptr<dentry_t> dir, vfs_listdir_state_t *state, dentry_iterator_op add_record)
{
    ptr<dentry_t> d_parent = dir->parent;
    if (d_parent == nullptr)
        d_parent = root_dentry;

    MOS_ASSERT(d_parent->inode != NULL);
    MOS_ASSERT(dir->inode);

    add_record(state, dir->inode->ino, ".", FILE_TYPE_DIRECTORY);
    add_record(state, d_parent->inode->ino, "..", FILE_TYPE_DIRECTORY);

    for (const auto &child : dir->children)
    {
        if (child->inode)
            add_record(state, child->inode->ino, child->name, child->inode->type);
    }
}

mos::string_view vfs_basename(mos::string_view path)
{
    if (path.empty())
        return {};

    // Find the last '/' character in the path
    size_t last_slash = path.find_last_of('/');
    if (last_slash == mos::string::npos)
        return path; // No slashes, return the whole path

    // Return the substring after the last slash
    return path.substr(last_slash + 1);
}
