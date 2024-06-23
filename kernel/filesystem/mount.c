// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/mount.h"

#include "mos/filesystem/dentry.h"
#include "mos/filesystem/vfs.h"
#include "mos/misc/setup.h"

#include <mos/lib/structures/hashmap_common.h>
#include <mos_stdlib.h>

#define VFS_MOUNTPOINT_MAP_SIZE 256
static hashmap_t vfs_mountpoint_map = { 0 }; // dentry_t -> mount_t

static void mountpoint_map_init(void)
{
    hashmap_init(&vfs_mountpoint_map, VFS_MOUNTPOINT_MAP_SIZE, hashmap_identity_hash, hashmap_simple_key_compare);
}
MOS_INIT(PRE_VFS, mountpoint_map_init);
list_head vfs_mountpoint_list = LIST_HEAD_INIT(vfs_mountpoint_list);

/**
 * @brief Given a mounted root dentry, return the mountpoint dentry that points to it
 *
 * @param dentry The mounted root dentry
 * @return dentry_t* The mountpoint dentry
 */
dentry_t *dentry_root_get_mountpoint(dentry_t *dentry)
{
    MOS_ASSERT(dentry);
    MOS_ASSERT_X(dentry->name == NULL, "mounted root should not have a name");

    if (dentry == root_dentry)
        return dentry; // the root dentry is its own mountpoint

    dentry_t *parent = dentry_parent(dentry);
    if (parent == NULL)
    {
        // root for some other fs trees
        return NULL;
    }

    tree_foreach_child(dentry_t, child, parent)
    {
        if (child->is_mountpoint)
        {
            mount_t *mount = dentry_get_mount(child);
            if (mount->root == dentry)
                return child;
        }
    }

    return NULL; // not found, possibly just have been unmounted
}

mount_t *dentry_get_mount(const dentry_t *dentry)
{
    if (!dentry->is_mountpoint)
    {
        mos_warn("dentry is not a mountpoint");
        return NULL;
    }

    mount_t *mount = hashmap_get(&vfs_mountpoint_map, (ptr_t) dentry);
    if (mount == NULL)
    {
        mos_warn("mountpoint not found");
        return NULL;
    }

    // otherwise the mountpoint must match the dentry
    MOS_ASSERT(mount->mountpoint == dentry);
    return mount;
}

bool dentry_mount(dentry_t *mountpoint, dentry_t *root, filesystem_t *fs)
{
    MOS_ASSERT_X(root->name == NULL, "mountpoint already has a name");
    MOS_ASSERT_X(dentry_parent(root) == NULL, "mountpoint already has a parent");

    dentry_ref(root);

    tree_node(root)->parent = NULL;
    if (dentry_parent(mountpoint))
        tree_node(root)->parent = tree_node(dentry_parent(mountpoint));

    mountpoint->is_mountpoint = true;

    mount_t *mount = kmalloc(mount_cache);
    linked_list_init(list_node(mount));
    list_node_append(&vfs_mountpoint_list, list_node(mount));
    mount->root = root;
    mount->superblock = root->inode->superblock;
    mount->mountpoint = mountpoint;
    mount->fs = fs;

    if (hashmap_put(&vfs_mountpoint_map, (ptr_t) mountpoint, mount) != NULL)
    {
        mos_warn("failed to insert mountpoint into hashmap");
        return false;
    }

    return true;
}

// remove root from the mount tree
dentry_t *dentry_unmount(dentry_t *root)
{
    mount_t *mount = dentry_get_mount(dentry_root_get_mountpoint(root));
    if (mount == NULL)
        return NULL;

    dentry_t *mountpoint = mount->mountpoint;

    hashmap_remove(&vfs_mountpoint_map, (ptr_t) mount->mountpoint);
    list_node_remove(list_node(mount));
    kfree(mount);
    mountpoint->is_mountpoint = false;
    return mountpoint;
}
