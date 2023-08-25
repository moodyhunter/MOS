// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/vfs_utils.h"

#include "mos/filesystem/vfs_types.h"
#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"

#include <stdlib.h>

static struct
{
    slab_t *inode_cache;
} caches = { 0 };

SLAB_AUTOINIT("inode", caches.inode_cache, inode_t);

void inode_init(inode_t *inode, superblock_t *sb, u64 ino, file_type_t type)
{
    inode->superblock = sb;
    inode->ino = ino;
    inode->type = type;
    inode->file_ops = NULL;
    inode->nlinks = 1;
    inode->perm = 0;
    inode->private = NULL;
}

inode_t *inode_create(superblock_t *sb, u64 ino, file_type_t type)
{
    inode_t *inode = kmemcache_alloc(caches.inode_cache);
    inode_init(inode, sb, ino, type);
    return inode;
}
