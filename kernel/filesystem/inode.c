// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/inode.h"

#include "mos/filesystem/vfs_types.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/syslog/printk.h"

#include <mos/lib/structures/hashmap_common.h>
#include <mos_stdlib.h>

slab_t *inode_cache;
SLAB_AUTOINIT("inode", inode_cache, inode_t);

static bool do_flush_and_drop_cache_page(const uintn key, void *value, void *data)
{
    MOS_UNUSED(key);
    MOS_UNUSED(value);
    MOS_UNUSED(data);
    return true;
    // phyframe_t *page = value;
    // mmstat_dec1(MEM_PAGECACHE);
    // pmm_free(page);
}

static bool vfs_generic_inode_drop(inode_t *inode)
{
    MOS_UNUSED(inode);
    kfree(inode);
    return true;
}

static bool inode_try_drop(inode_t *inode)
{
    if (inode->refcount == 0 && inode->nlinks == 0)
    {
        // drop the inode
        inode_cache_t *icache = &inode->cache;
        hashmap_foreach(&icache->pages, do_flush_and_drop_cache_page, NULL);

        bool dropped = false;
        if (inode->superblock->ops && inode->superblock->ops->drop_inode)
            dropped = inode->superblock->ops->drop_inode(inode);
        else
            dropped = vfs_generic_inode_drop(inode);

        if (!dropped)
            pr_warn("inode %p has 0 refcount and 0 nlinks, but could not be dropped", (void *) inode);

        return dropped;
    }

    return false;
}

void inode_init(inode_t *inode, superblock_t *sb, u64 ino, file_type_t type)
{
    inode->superblock = sb;
    inode->ino = ino;
    inode->type = type;
    inode->file_ops = NULL;
    inode->nlinks = 1;
    inode->perm = 0;
    inode->private = NULL;
    inode->refcount = 0;

    hashmap_init(&inode->cache.pages, MOS_INODE_CACHE_HASHMAP_SIZE, hashmap_identity_hash, hashmap_simple_key_compare);
    inode->cache.owner = inode;
}

inode_t *inode_create(superblock_t *sb, u64 ino, file_type_t type)
{
    inode_t *inode = kmalloc(inode_cache);
    inode_init(inode, sb, ino, type);
    return inode;
}

void inode_ref(inode_t *inode)
{
    MOS_ASSERT(inode);
    inode->refcount++;
}

bool inode_unref(inode_t *inode)
{
    MOS_ASSERT(inode);
    MOS_ASSERT(inode->refcount > 0);
    inode->refcount--;
    return inode_try_drop(inode);
}

bool inode_unlink(inode_t *dir, dentry_t *dentry)
{
    inode_t *inode = dentry->inode;
    MOS_ASSERT(dir && inode);
    MOS_ASSERT(inode->nlinks > 0);
    inode->nlinks--;
    if (dir->ops->unlink)
        dir->ops->unlink(dir, dentry);
    return inode_try_drop(dentry->inode);
}
