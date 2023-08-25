// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/vfs_utils.h"

#include "mos/filesystem/vfs_types.h"
#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"

#include <stdlib.h>
#include <string.h>

static struct
{
    slab_t *inode_cache;
    slab_t *dentry_cache;
} caches = { 0 };

SLAB_AUTOINIT("inode", caches.inode_cache, inode_t);
SLAB_AUTOINIT("dentry", caches.dentry_cache, dentry_t);

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
    inode_t *inode = kmalloc(caches.inode_cache);
    inode_init(inode, sb, ino, type);
    return inode;
}

dentry_t *dentry_create(superblock_t *sb, dentry_t *parent, const char *name)
{
    dentry_t *dentry = kmalloc(caches.dentry_cache);
    dentry->superblock = sb;
    linked_list_init(&tree_node(dentry)->children);
    linked_list_init(&tree_node(dentry)->list_node);

    if (name)
        dentry->name = strdup(name);

    if (parent)
    {
        tree_add_child(tree_node(parent), tree_node(dentry));
        dentry->superblock = parent->superblock;
    }

    return dentry;
}
