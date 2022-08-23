// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/mount.h"

#include "lib/hashmap.h"
#include "mos/filesystem/fs_fwd.h"
#include "mos/filesystem/path.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

static mountpoint_t *rootmp = NULL;

void kmount_add_mount(mountpoint_t *new_mp)
{
    if (rootmp == NULL)
    {
        pr_info2("root filesystem mounted");
        rootmp = new_mp;
        return;
    }

    mountpoint_t *parent = kmount_find_under(rootmp, new_mp->path);
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

mountpoint_t *kmount(path_t *path, filesystem_t *fs, blockdev_t *blockdev)
{
    pr_info("mount '%s' on '%s' (blockdev '%s')", fs->name, path->name, blockdev->name);
    mountpoint_t *new_mp = kcalloc(1, sizeof(mountpoint_t));

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

    kmount_add_mount(new_mp);
    return new_mp;
}

bool kunmount(mountpoint_t *mountpoint)
{
    if (mountpoint->refcount.atomic > 0)
    {
        mos_warn("mountpoint %s still has %llu references", mountpoint->path->name, mountpoint->refcount.atomic);
        return false;
    }
    if (mountpoint->children_count > 0)
    {
        mos_warn("mountpoint %s still has %u children", mountpoint->path->name, mountpoint->children_count);
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

mountpoint_t *kmount_find(path_t *path)
{
    if (unlikely(rootmp == NULL))
        return NULL;

    return kmount_find_under(rootmp, path);
}

mountpoint_t *kmount_find_under(mountpoint_t *mp, path_t *path)
{
    if (mp->children == NULL)
        return NULL;

    mountpoint_t *match = NULL;
    for (size_t i = 0; i < mp->children_count; i++)
    {
        mountpoint_t *child = mp->children[i];
        if (child->path->name[0] == '/')
            continue;
        if (path_compare(child->path, path) == 0)
        {
            match = child;
            return kmount_find_under(child, path);
        }
    }
    return match;
}
