// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/vfs_utils.h"

#include "mos/filesystem/page_cache.h"
#include "mos/filesystem/vfs_types.h"
#include "mos/mm/mm.h"
#include "mos/mm/physical/pmm.h"
#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"

#include <mos/lib/structures/hashmap_common.h>
#include <mos/types.h>
#include <mos_stdlib.h>
#include <mos_string.h>

slab_t *dentry_cache;
SLAB_AUTOINIT("dentry", dentry_cache, dentry_t);

dentry_t *dentry_create(superblock_t *sb, dentry_t *parent, const char *name)
{
    dentry_t *dentry = kmalloc(dentry_cache);
    dentry->superblock = sb;
    tree_node_init(tree_node(dentry));

    if (name)
        dentry->name = strdup(name);

    if (parent)
    {
        tree_add_child(tree_node(parent), tree_node(dentry));
        dentry->superblock = parent->superblock;
    }

    return dentry;
}

bool simple_page_write_begin(inode_cache_t *icache, off_t offset, size_t size, phyframe_t **page, void **private)
{
    MOS_UNUSED(size);
    *page = pagecache_get_page_for_write(icache, offset / MOS_PAGE_SIZE);
    if (!*page)
        return false;

    *private = NULL;
    return true;
}

void simple_page_write_end(inode_cache_t *icache, off_t offset, size_t size, phyframe_t *page, void *private)
{
    MOS_UNUSED(page);
    MOS_UNUSED(private);

    // also update the inode's size
    if (offset + size > icache->owner->size)
        icache->owner->size = offset + size;
}

// read from the page cache, the size and offset are already validated to be in the file's bounds
ssize_t vfs_generic_read(const file_t *file, void *buf, size_t size, off_t offset)
{
    // cap the read size to the file's size
    size = MIN(size, file->dentry->inode->size - offset);
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

size_t vfs_generic_iterate_dir(const dentry_t *dir, dir_iterator_state_t *state, dentry_iterator_op op)
{
    size_t written = 0;

    size_t i = state->i - state->start_nth;
    tree_foreach_child(dentry_t, child, dir)
    {
        if (child->inode == NULL)
            continue;

        // skip entries until we reach the nth one
        if (state->i != i++)
            continue;

        const size_t w = op(state, child->inode->ino, child->name, strlen(child->name), child->inode->type);
        if (w == 0)
            break;
        written += w;
    }

    return written;
}
