// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/mount.hpp"

#include "mos/filesystem/dentry.hpp"
#include "mos/filesystem/vfs.hpp"

#include <mos/hashmap.hpp>
#include <mos/shared_ptr.hpp>
#include <mos_stdlib.hpp>

#define VFS_MOUNTPOINT_MAP_SIZE 256
mos::HashMap<ptr<dentry_t>, ptr<mount_t>> vfs_mountpoint_map; // dentry_t -> mount_t

/**
 * @brief Given a mounted root dentry, return the mountpoint dentry that points to it
 *
 * @param dentry The mounted root dentry
 * @return dentry_t* The mountpoint dentry
 */
ptr<dentry_t> dentry_root_get_mountpoint(const ptr<dentry_t> dentry)
{
    MOS_ASSERT(dentry);
    MOS_ASSERT_X(dentry->name.empty(), "mounted root should not have a name");

    if (dentry == root_dentry)
        return dentry; // the root dentry is its own mountpoint

    ptr<dentry_t> parent = dentry->parent;
    if (parent == nullptr)
    {
        // root for some other fs trees
        return nullptr;
    }

    for (const auto &child : parent->children)
    {
        if (child->is_mountpoint)
        {
            const auto mount = dentry_get_mount(child);
            if (mount->root == dentry)
                return child;
        }
    }

    return nullptr; // not found, possibly just have been unmounted
}

ptr<mount_t> dentry_get_mount(const ptr<dentry_t> dentry)
{
    if (!dentry->is_mountpoint)
    {
        mos_warn("dentry is not a mountpoint");
        return NULL;
    }

    const auto pmount = vfs_mountpoint_map.get(dentry);
    if (!pmount.has_value())
    {
        mos_warn("mountpoint not found");
        return NULL;
    }

    // otherwise the mountpoint must match the dentry
    MOS_ASSERT((*pmount)->mountpoint == dentry);
    return *pmount;
}

bool dentry_mount(ptr<dentry_t> mountpoint, ptr<dentry_t> root, filesystem_t *fs)
{
    MOS_ASSERT_X(root->name.empty(), "mountpoint already has a name");

    if (root->parent)
    {
        mWarn << "dentry mount root already has a parent";
        return false;
    }

    root->parent = mountpoint->parent;

    mountpoint->is_mountpoint = true;

    auto mount = mos::make_shared<mount_t>();
    mount->root = root;
    mount->superblock = root->inode->superblock;
    mount->mountpoint = mountpoint;
    mount->fs = fs;
    vfs_mountpoint_map.insert(mountpoint, std::move(mount));
    return true;
}

// remove root from the mount tree
ptr<dentry_t> dentry_unmount(ptr<dentry_t> root)
{
    const auto mount = dentry_get_mount(dentry_root_get_mountpoint(root));
    if (!mount)
        return NULL;

    ptr<dentry_t> mountpoint = mount->mountpoint;

    vfs_mountpoint_map.remove(mount->mountpoint);
    // delete mount; // TODO: verify if mount is correctly deleted
    MOS_ASSERT(mount.use_count() == 1);
    mountpoint->is_mountpoint = false;
    return mountpoint;
}
