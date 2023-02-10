#include "mos/filesystem/vfs.h"

#include "lib/string.h"
#include "lib/structures/list.h"
#include "lib/sync/spinlock.h"
#include "mos/filesystem/dentry.h"
#include "mos/filesystem/fs_types.h"
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
    file_get_ops(file)->flush(file);
    tree_trace_to_root(tree_node(file->dentry), path_treeop_decrement_refcount);
    kfree(file);
}

size_t vfs_io_ops_read(io_t *io, void *buf, size_t count)
{
    file_t *file = container_of(io, file_t, io);
    return file_get_ops(file)->read(file, buf, count);
}

size_t vfs_io_ops_write(io_t *io, const void *buf, size_t count)
{
    file_t *file = container_of(io, file_t, io);
    return file_get_ops(file)->write(file, buf, count);
}

io_op_t fs_io_ops = {
    .read = vfs_io_ops_read,
    .write = vfs_io_ops_write,
    .close = vfs_io_ops_close,
};
// END: filesystem's io_t operations

list_node_t vfs_fs_list = LIST_HEAD_INIT(vfs_fs_list); // filesystem_t
spinlock_t vfs_fs_list_lock = SPINLOCK_INIT;

void vfs_register_filesystem(filesystem_t *fs)
{
    spinlock_acquire(&vfs_fs_list_lock);
    list_node_append(&vfs_fs_list, list_node(fs));
    spinlock_release(&vfs_fs_list_lock);

    pr_info("filesystem '%s' registered", fs->name);
}

#define FD_CWD -69

dentry_t *root_dentry = NULL;

static filesystem_t *vfs_find_filesystem(const char *name)
{
    filesystem_t *fs_found = NULL;
    spinlock_acquire(&vfs_fs_list_lock);
    list_foreach(filesystem_t, fs, vfs_fs_list)
    {
        if (strcmp(fs->name, name) == 0)
        {
            fs_found = fs;
            break;
        }
    }
    spinlock_release(&vfs_fs_list_lock);
    return fs_found;
}

void vfs_init(void)
{
    pr_info("initializing the Virtual File System (VFS) subsystem...");
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

    if (unlikely(root_dentry == NULL))
    {
        // special case: mount root filesystem
        MOS_ASSERT(strcmp(path, "/") == 0);
        root_dentry = real_fs->ops->mount(real_fs, device, options);
        if (root_dentry == NULL)
        {
            mos_warn("failed to mount root filesystem");
            return false;
        }

        return true;
    }

    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(FD_CWD);
    dentry_t *mount_point = dentry_resolve(base, root_dentry, path, RESOLVE_DIRECTORY | RESOLVE_FOLLOW_SYMLINK);
    if (unlikely(mount_point == NULL))
    {
        mos_warn("mount point does not exist");
        return false;
    }

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

file_t *vfs_do_open_relative(dentry_t *base, const char *path, file_open_flags flags)
{
    if (base == NULL)
        return NULL;

    dentry_t *entry = dentry_resolve(base, root_dentry, path, RESOLVE_FILE | RESOLVE_FOLLOW_SYMLINK);

    file_t *file = kzalloc(sizeof(file_t));
    file->dentry = entry;

    bool opened = file_get_ops(file)->open(file->dentry->inode, file);

    MOS_UNREACHABLE();
}

file_t *vfs_open(const char *path, file_open_flags flags)
{
    return vfs_openat(FD_CWD, path, flags);
}

file_t *vfs_openat(int fd, const char *path, file_open_flags flags)
{
    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(FD_CWD);
    file_t *file = vfs_do_open_relative(base, path, flags);
    return file;
}

bool vfs_stat(const char *path, file_stat_t *restrict stat)
{
    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(FD_CWD);
    dentry_t *file = dentry_resolve(base, root_dentry, path, RESOLVE_FILE);
    if (file == NULL)
        return false;

    mos_warn("TODO: implement vfs_stat()");
    *stat = file->inode->stat;
    return true;
}
