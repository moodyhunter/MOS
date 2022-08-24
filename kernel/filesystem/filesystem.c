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

void vfs_io_ops_close(io_t *io)
{
    pr_info2("vfs_io_ops_close(%p)", (void *) io);
}

size_t vfs_io_ops_read(io_t *io, void *buf, size_t count)
{
    file_t *file = container_of(io, file_t, io);
    return vfs_read(file, buf, count);
}

size_t vfs_io_ops_write(io_t *io, const void *buf, size_t count)
{
    file_t *file = container_of(io, file_t, io);
    return vfs_write(file, buf, count);
}

io_ops_t fs_io_ops = {
    .read = vfs_io_ops_read,
    .write = vfs_io_ops_write,
    .close = vfs_io_ops_close,
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

file_t *vfs_open(const char *path, file_open_flags flags)
{
    fsnode_t *node = path_find_fsnode(path);
    file_t *file = kcalloc(1, sizeof(file_t));
    file->fsnode = node;
    bool opened = vfs_path_open(node, flags, file);
    if (!opened)
    {
        kfree(node);
        kfree(file);
        return NULL;
    }
    return file;
}

bool vfs_stat(const char *path, file_stat_t *restrict stat)
{
    fsnode_t *p = path_find_fsnode(path);
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
    fsnode_t *p = path_find_fsnode(path);
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

bool vfs_path_open(fsnode_t *path, file_open_flags flags, file_t *file)
{
    mountpoint_t *mp = kmount_find_mp(path);
    if (mp == NULL)
    {
        mos_warn("no filesystem mounted at %s", path->name);
        return NULL;
    }

    mos_debug("opening file %s on fs: %s, blockdev: %s", path->name, mp->fs->name, mp->dev->name);

    file_stat_t stat;
    if (!mp->fs->op_stat(mp, path, &stat))
    {
        mos_warn("stat failed for %s", path->name);
        return NULL;
    }

    if (stat.type == FILE_TYPE_SYMLINK)
    {
        if (flags & OPEN_NO_FOLLOW)
            goto _continue;

        // TODO: follow symlinks
    }

_continue:;

    if (!mp->fs->op_open(mp, path, flags, file))
    {
        mos_warn("failed to open file %s", path->name);
        return NULL;
    }

    tree_trace_to_root(tree_node(path), path_treeop_increment_refcount);
    file->io.ops = &fs_io_ops;
    file->io.size = stat.size;
    return true;
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

size_t vfs_read(file_t *file, void *buf, size_t count)
{
    mountpoint_t *mp = kmount_find_mp(file->fsnode);
    if (mp == NULL)
    {
        mos_warn("no filesystem mounted at %s", file->fsnode->name);
        return -1;
    }
    size_t result = mp->fs->op_read(mp->dev, file, buf, count);
    return result;
}

size_t vfs_write(file_t *file, const void *buf, size_t count)
{
    mountpoint_t *mp = kmount_find_mp(file->fsnode);
    if (mp == NULL)
    {
        mos_warn("no filesystem mounted at %s", file->fsnode->name);
        return -1;
    }
    size_t result = mp->fs->op_write(mp->dev, file, buf, count);
    return result;
}

void file_io_cleanup(io_t *io)
{
    // the file has already been closed, so we can just free the fsnode
    file_t *file = container_of(io, file_t, io);
    tree_trace_to_root(tree_node(file->fsnode), path_treeop_decrement_refcount);
    if (file->fsnode)
        kfree(file->fsnode);
    kfree(file);
}
