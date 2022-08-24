// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/filesystem.h"

#include "lib/string.h"
#include "lib/structures/tree.h"
#include "mos/device/block.h"
#include "mos/filesystem/mount.h"
#include "mos/filesystem/pathutils.h"
#include "mos/io/io.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"

fsnode_t root_path = {
    .name = "/",
    .tree_node = {
        .parent = NULL,
        .n_children = 0,
        .children = NULL,
    },
};

void path_node_get_name(const tree_node_t *node, char **name, size_t *name_len)
{
    fsnode_t *path = tree_entry(node, fsnode_t);
    *name = (char *) path->name;
    *name_len = strlen(path->name);
}

tree_op_t path_tree_op = {
    .get_node_name = path_node_get_name,
};

fsnode_t *vfs_open(const char *path, file_open_flags flags)
{
    fsnode_t *p = path_contruct(path);
    bool opened = vfs_path_open(p, flags);
    if (!opened)
    {
        kfree(p);
        return NULL;
    }
    return p;
}

bool vfs_stat(const char *path, file_stat_t *restrict stat)
{
    fsnode_t *p = path_contruct(path);
    bool opened = vfs_path_stat(p, stat);
    if (!opened)
    {
        kfree(p);
        return NULL;
    }
    return p;
}

fsnode_t *vfs_readlink(const char *path)
{
    fsnode_t *p = path_contruct(path);
    fsnode_t *target = kmalloc(sizeof(fsnode_t));
    bool opened = vfs_path_readlink(p, &target);
    if (!opened)
    {
        kfree(p);
        kfree(target);
        return NULL;
    }
    return target;
}

bool vfs_path_open(fsnode_t *path, file_open_flags flags)
{
    mountpoint_t *mp = kmount_find_mp(path);
    if (mp == NULL)
    {
        mos_warn("no filesystem mounted at %s", path->name);
        return NULL;
    }

    fsnode_t *file = kcalloc(1, sizeof(fsnode_t));
    file->io.type = IO_TYPE_FILE;

    mos_debug("opening file %s on fs: %s, blockdev: %s", path->name, mp->fs->name, mp->dev->name);

    file_stat_t stat;
    if (!mp->fs->op_stat(mp, path, &stat))
    {
        mos_warn("stat failed for %s", path->name);
        kfree(file);
        return NULL;
    }

    if (stat.type == FILE_TYPE_SYMLINK)
    {
        if (flags & OPEN_NO_FOLLOW)
            goto _continue;

        char *target = kcalloc(stat.size + 1, sizeof(char));

        if (!mp->fs->op_readlink(mp, path, target, stat.size))
        {
            mos_warn("readlink failed for %s", path->name);
            kfree(file);
            kfree(target);
            return NULL;
        }

        mos_debug("readlink %s -> %s", path->name, target);
        // fsnode_t *link_target = path_concat_n(path_parent(path), target);
        // vfs_path_open(link_target, flags);
    }

_continue:;

    if (!mp->fs->op_open(mp, path, flags, file))
    {
        mos_warn("failed to open file %s", path->name);
        kfree(file);
        return NULL;
    }
    tree_trace_to_root(tree_node(path), path_treeop_increment_refcount);
    return file;
}

bool vfs_path_readlink(fsnode_t *path, fsnode_t **link)
{
    mountpoint_t *mp = kmount_find_mp(path);
    if (mp == NULL)
    {
        mos_warn("no filesystem mounted at %s", path->name);
        return false;
    }

    file_stat_t stat;
    if (!mp->fs->op_stat(mp, path, &stat))
    {
        mos_warn("stat failed for %s", path->name);
        return false;
    }

    if (stat.type != FILE_TYPE_SYMLINK)
    {
        mos_warn("%s is not a symlink", path->name);
        return false;
    }

    char target[stat.size + 1];
    if (!mp->fs->op_readlink(mp, path, target, stat.size))
    {
        mos_warn("readlink failed for %s", path->name);
        return false;
    }
    target[stat.size] = '\0';

    // ! 'path' is always a symlink (aka, a file), so we have to get its parent
    bool resolved = path_resolve(path_parent(path), target, link);
    return resolved;
}

bool vfs_path_stat(fsnode_t *path, file_stat_t *restrict stat)
{
    mountpoint_t *mp = kmount_find_mp(path);
    if (mp == NULL)
    {
        mos_warn("no filesystem mounted at %s", path->name);
        return -1;
    }
    bool result = mp->fs->op_stat(mp, path, stat);
    return result;
}
