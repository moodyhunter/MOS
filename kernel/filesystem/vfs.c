#include "mos/filesystem/vfs.h"

#include "lib/string.h"
#include "lib/structures/list.h"
#include "lib/sync/spinlock.h"
#include "mos/filesystem/dentry.h"
#include "mos/filesystem/fs_types.h"
#include "mos/filesystem/mount.h"
#include "mos/filesystem/pathutils.h"
#include "mos/io/io.h"
#include "mos/mm/kmalloc.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/types.h"

// BEGIN: filesystem's io_t operations
void vfs_io_ops_close(io_t *io)
{
    file_t *file = container_of(io, file_t, io);
    file->ops->flush(file);
    tree_trace_to_root(tree_node(file->dentry), path_treeop_decrement_refcount);
    kfree(file);
}

size_t vfs_io_ops_read(io_t *io, void *buf, size_t count)
{
    file_t *file = container_of(io, file_t, io);
    return file->ops->read(file, buf, count);
}

size_t vfs_io_ops_write(io_t *io, const void *buf, size_t count)
{
    file_t *file = container_of(io, file_t, io);
    return file->ops->write(file, buf, count);
}

io_op_t fs_io_ops = {
    .read = vfs_io_ops_read,
    .write = vfs_io_ops_write,
    .close = vfs_io_ops_close,
};
// END: filesystem's io_t operations

list_node_t vfs_fs_list = LIST_HEAD_INIT(vfs_fs_list); // filesystem_t
spinlock_t vfs_fs_list_lock = SPINLOCK_INIT;

#define FD_CWD -69
extern dentry_t *root_dentry;

static filesystem_t *vfs_find_filesystem(const char *name)
{
    list_foreach(filesystem_t, fs, vfs_fs_list)
    {
        if (strcmp(fs->name, name) == 0)
            return fs;
    }
    return NULL;
}

void vfs_init(void)
{
    dentry_init();
}

bool vfs_mount(const char *device, const char *path, const char *fs, const char *options)
{
    filesystem_t *real_fs = vfs_find_filesystem(fs);
    if (real_fs == NULL)
    {
        mos_warn("filesystem '%s' not found", fs);
        return false;
    }

    char *last_segment;
    dentry_t *parent = dentry_lookup_parent(root_dentry, root_dentry, path, &last_segment);
    if (parent == NULL)
    {
        mos_warn("mountpoint does not exist");
        return false; // mount point does not exist
    }

    dentry_t *mount_point = dentry_resolve_handle_last_segment(parent, last_segment, LASTSEG_DIRECTORY | LASTSEG_FOLLOW_SYMLINK);

    dentry_t *mounted_root = real_fs->ops->mount(real_fs, device, options);
    if (mounted_root == NULL)
    {
        mos_warn("failed to mount filesystem");
        return false;
    }

    bool mounted = dentry_mount(mount_point, mounted_root);
    if (unlikely(!mounted))
    {
        mos_warn("failed to mount filesystem");
        return false;
    }

    return true;
}

file_t *vfs_do_open_at(dentry_t *base, const char *path, file_open_flags flags)
{
    if (base == NULL)
        return NULL;

    char *last_segment;
    dentry_t *entry = dentry_lookup_parent(base, root_dentry, path, &last_segment);

    if (entry == NULL)
        return NULL; // non-existent file

    file_t *file = kzalloc(sizeof(file_t));
    file->dentry = entry;
    file->ops = entry->inode->file_ops;

    bool opened = file->ops->open(file->dentry->inode, file);

    MOS_UNREACHABLE();
}

file_t *vfs_open(const char *path, file_open_flags flags)
{
    dentry_t *base = root_dentry;
    file_t *file = vfs_do_open_at(base, path, flags);
    return file;
}

file_t *vfs_openat(int fd, const char *path, file_open_flags flags)
{
    dentry_t *entry = dentry_from_fd(fd);
    file_t *file = vfs_do_open_at(entry, path, flags);
    return file;
}

// bool vfs_stat(const char *path, file_stat_t *restrict stat)
// {
//     fsnode_t *p = path_find_fsnode(path);
//     bool opened = vfs_path_stat(p, stat);
//     if (!opened)
//     {
//         kfree(p);
//         return NULL;
//     }
//     return p;
// }

// const char *vfs_readlink(const char *path)
// {
//     inode_t *p = path_find_fsnode(path);
//     inode_t *target = kmalloc(sizeof(inode_t));
//     bool opened = vfs_path_readlink(p, &target);
//     if (!opened)
//     {
//         kfree(p);
//         kfree(target);
//         return NULL;
//     }
//     return target;
// }

// static bool vfs_path_open(const dentry_t *entry, file_open_flags flags, file_t *file)
// {
//     mountpoint_t *mp = kmount_find_mp(path);
//     if (mp == NULL)
//     {
//         mos_warn("no filesystem mounted at %s", path->name);
//         return NULL;
//     }

//     mos_debug(fs, "opening file %s on fs: %s, blockdev: %s", path->name, mp->fs->name, mp->dev->name);

//     file_stat_t stat;
//     if (!mp->fs->op_stat(mp, path, &stat))
//     {
//         mos_warn("stat failed for %s", path->name);
//         return NULL;
//     }

//     if (stat.type == FILE_TYPE_SYMLINK)
//     {
//         if (flags & FILE_OPEN_SYMLINK_NO_FOLLOW)
//             goto _continue;

//         // TODO: follow symlinks
//     }

// _continue:;

//     if (!mp->fs->op_open(mp, path, flags, file))
//     {
//         mos_warn("failed to open file %s", path->name);
//         return NULL;
//     }

//     tree_trace_to_root(tree_node(path), path_treeop_increment_refcount);
//     io_init(&file->io, (flags & FILE_OPEN_READ) | (flags & FILE_OPEN_WRITE), &fs_io_ops);
//     return true;
// }

// static bool vfs_path_readlink(const dentry_t *entry, char *buf, size_t bufsize)
// {
//     mountpoint_t *mp = kmount_find_mp(path);
//     if (mp == NULL)
//     {
//         mos_warn("no filesystem mounted at %s", path->name);
//         return false;
//     }

//     if (!entry->inode->ops->readlink(entry, buf, bufsize))
//     {
//         mos_warn("readlink failed for %s", path->name);
//         return false;
//     }

//     return true;
// }

// static bool vfs_path_stat(const fsnode_t *path, file_stat_t *restrict stat)
// {
//     mountpoint_t *mp = kmount_find_mp(path);
//     if (mp == NULL)
//     {
//         mos_warn("no filesystem mounted at %s", path->name);
//         return -1;
//     }
//     bool result = mp->fs->op_stat(mp, path, stat);
//     return result;
// }
