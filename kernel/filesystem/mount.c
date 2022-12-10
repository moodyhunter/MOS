// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/mount.h"

#include "lib/string.h"
#include "mos/device/block.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

static mountpoint_t *rootmp = NULL;

void kmount_add_mp(mountpoint_t *new_mp)
{
    if (rootmp == NULL)
    {
        pr_info2("root filesystem mounted");
        rootmp = new_mp;
        return;
    }

    mountpoint_t *parent = kmount_find_submp(rootmp, new_mp->path);
    if (parent == NULL)
    {
        mos_warn("mountpoint %s cannot be placed in tree", new_mp->path->name);
        MOS_UNREACHABLE();
        return;
    }

    new_mp->parent = parent;
    parent->children = krealloc(parent->children, sizeof(mountpoint_t *) * (parent->children_count + 1));
    parent->children[parent->children_count] = new_mp;
}

mountpoint_t *kmount(fsnode_t *path, filesystem_t *fs, blockdev_t *blockdev)
{
    MOS_ASSERT(path != NULL);
    MOS_ASSERT(fs != NULL);
    MOS_ASSERT(blockdev != NULL);

    pr_info("mounting '%s' on '%s' (blockdev '%s')", fs->name, path->name, blockdev->name);
    mountpoint_t *new_mp = kzalloc(sizeof(mountpoint_t));

    if (!new_mp)
        return NULL;
    new_mp->path = path;
    new_mp->fs = fs;
    new_mp->dev = blockdev;
    new_mp->children = NULL;

    if (!fs->op_mount(blockdev, path))
    {
        kfree(new_mp);
        mos_warn("failed to mount filesystem %s at %s", fs->name, path->name);
        return NULL;
    }

    kmount_add_mp(new_mp);
    return new_mp;
}

bool kunmount(mountpoint_t *mountpoint)
{
    if (mountpoint == NULL)
        return false;

    u64 refs = refcount_get(&mountpoint->refcount);
    if (refs > 0)
    {
        mos_warn("mountpoint %s still has %llu references", mountpoint->path->name, refs);
        return false;
    }
    if (mountpoint->children_count > 0)
    {
        mos_warn("mountpoint %s still has %zu children", mountpoint->path->name, mountpoint->children_count);
        return false;
    }

    if (!mountpoint->fs->op_unmount(mountpoint))
    {
        mos_warn("failed to unmount filesystem %s at %s", mountpoint->fs->name, mountpoint->path->name);
        return false;
    }
    if (unlikely(mountpoint == rootmp))
    {
        pr_warn("unmounting root filesystem");
        rootmp = NULL;
    }
    kfree(mountpoint);
    return true;
}

mountpoint_t *kmount_find_mp(fsnode_t *path)
{
    if (unlikely(rootmp == NULL))
        return NULL;

    return kmount_find_submp(rootmp, path);
}

mountpoint_t *kmount_find_submp(mountpoint_t *mp, fsnode_t *path)
{
    if (mp->children == NULL)
        return mp;

    mountpoint_t *match = NULL;
    for (size_t i = 0; i < mp->children_count; i++)
    {
        mountpoint_t *child = mp->children[i];
        if (child->path->name[0] == '/')
            continue;
        if (strcmp(child->path->name, path->name) == 0)
        {
            match = child;
            break;
        }
    }
    return match;
}
