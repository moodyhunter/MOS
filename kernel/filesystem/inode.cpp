// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/inode.hpp"

#include "mos/filesystem/page_cache.hpp"
#include "mos/filesystem/vfs_types.hpp"
#include "mos/syslog/printk.hpp"

#include <mos/lib/structures/hashmap_common.hpp>
#include <mos_stdlib.hpp>

static bool vfs_generic_inode_drop(inode_t *inode)
{
    MOS_UNUSED(inode);
    delete inode;
    return true;
}

static bool inode_try_drop(inode_t *inode)
{
    if (inode->refcount == 0 && inode->nlinks == 0)
    {
        pr_dinfo2(vfs, "inode %p has 0 refcount and 0 nlinks, dropping", (void *) inode);

        // drop the inode
        mutex_acquire(&inode->cache.lock);
        pagecache_flush_or_drop_all(&inode->cache, true);
        mutex_release(&inode->cache.lock);

        bool dropped = false;
        if (inode->superblock->ops && inode->superblock->ops->drop_inode)
            dropped = inode->superblock->ops->drop_inode(inode);
        else
            dropped = vfs_generic_inode_drop(inode);

        if (!dropped)
            pr_warn("inode %p has 0 refcount and 0 nlinks, but failed to be dropped", (void *) inode);

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
    inode->private_data = NULL;
    inode->refcount = 0;
    inode->cache.owner = inode;
    inode->cache.lock = 0;
}

inode_t *inode_create(superblock_t *sb, u64 ino, file_type_t type)
{
    inode_t *inode = mos::create<inode_t>();
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
    bool ok = true;
    if (dir->ops && dir->ops->unlink)
        ok = dir->ops->unlink(dir, dentry);

    if (!ok)
    {
        inode->nlinks++;
        return false;
    }

    const bool dropped = inode_try_drop(dentry->inode);
    MOS_ASSERT_X(!dropped, "inode %p was dropped accidentally, where dentry %p should be holding a reference", (void *) inode, (void *) dentry);

    return true;
}
