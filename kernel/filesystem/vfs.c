#include "mos/filesystem/vfs.h"

#include "lib/string.h"
#include "lib/structures/list.h"
#include "lib/structures/tree.h"
#include "lib/sync/spinlock.h"
#include "mos/filesystem/dentry.h"
#include "mos/filesystem/fs_types.h"
#include "mos/io/io.h"
#include "mos/mm/kmalloc.h"
#include "mos/mos_global.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/process.h"
#include "mos/types.h"

static list_node_t vfs_fs_list = LIST_HEAD_INIT(vfs_fs_list); // filesystem_t
static spinlock_t vfs_fs_list_lock = SPINLOCK_INIT;

dentry_t *root_dentry = NULL;

// BEGIN: filesystem's io_t operations
static void vfs_io_ops_close(io_t *io)
{
    file_t *file = container_of(io, file_t, io);
    const file_ops_t *file_ops = file_get_ops(file);
    if (file_ops->flush)
        file_ops->flush(file);
    dentry_unref(file->dentry);
    kfree(file);
}

static size_t vfs_io_ops_read(io_t *io, void *buf, size_t count)
{
    file_t *file = container_of(io, file_t, io);
    return file_get_ops(file)->read(file, buf, count);
}

static size_t vfs_io_ops_write(io_t *io, const void *buf, size_t count)
{
    file_t *file = container_of(io, file_t, io);
    return file_get_ops(file)->write(file, buf, count);
}

static io_op_t fs_io_ops = {
    .read = vfs_io_ops_read,
    .write = vfs_io_ops_write,
    .close = vfs_io_ops_close,
};
// END: filesystem's io_t operations

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

static bool vfs_verify_permissions(dentry_t *file_dentry, bool open, bool read, bool create, bool execute, bool write)
{
    MOS_ASSERT(file_dentry != NULL && file_dentry->inode != NULL);
    const file_perm_t file_perm = file_dentry->inode->stat.perm;

    // TODO: we are treating all users as root for now, only checks for execute permission
    MOS_UNUSED(open);
    MOS_UNUSED(read);
    MOS_UNUSED(create);
    MOS_UNUSED(write);

    if (execute && !(file_perm.owner.execute || file_perm.group.execute || file_perm.others.execute))
        return false; // execute permission denied

    return true;
}

static file_t *vfs_do_open_relative(dentry_t *base, const char *path, file_open_flags flags)
{
    if (base == NULL)
        return NULL;

    const bool may_create = flags & FILE_OPEN_CREATE;
    const bool read = flags & FILE_OPEN_READ;
    const bool write = flags & FILE_OPEN_WRITE;
    const bool execute = flags & FILE_OPEN_EXECUTE;
    const bool no_follow = flags & FILE_OPEN_NO_FOLLOW;

    lastseg_resolve_flags_t resolve_flags = RESOLVE_FILE | (no_follow ? 0 : RESOLVE_FOLLOW_SYMLINK) | (may_create ? RESOLVE_CREATE_IF_NONEXIST : 0);
    dentry_t *entry = dentry_resolve(base, root_dentry, path, resolve_flags);

    if (entry == NULL)
    {
        pr_warn("failed to resolve path '%s', c=%d, r=%d, e=%d, n=%d", path, may_create, read, execute, no_follow);
        return NULL;
    }

    if (!vfs_verify_permissions(entry, true, read, may_create, execute, write))
        return NULL;

    file_t *file = kzalloc(sizeof(file_t));
    file->dentry = entry;

    io_flags_t io_flags = (flags & FILE_OPEN_READ ? IO_READABLE : 0) | (flags & FILE_OPEN_WRITE ? IO_WRITABLE : 0) | IO_SEEKABLE;
    io_init(&file->io, io_flags, &fs_io_ops);

    const file_ops_t *ops = file_get_ops(file);
    if (ops->open)
    {
        bool opened = ops->open(file->dentry->inode, file);
        if (!opened)
        {
            pr_warn("failed to open file '%s'", path);
            kfree(file);
            return NULL;
        }
    }

    return file;
}

void vfs_init(void)
{
    pr_info("initializing the Virtual File System (VFS) subsystem...");
    dentry_init();
}

void vfs_register_filesystem(filesystem_t *fs)
{
    spinlock_acquire(&vfs_fs_list_lock);
    list_node_append(&vfs_fs_list, list_node(fs));
    spinlock_release(&vfs_fs_list_lock);

    pr_info("filesystem '%s' registered", fs->name);
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

file_t *vfs_open(const char *path, file_open_flags flags)
{
    return vfs_openat(FD_CWD, path, flags);
}

file_t *vfs_openat(int fd, const char *path, file_open_flags flags)
{
    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(fd);
    file_t *file = vfs_do_open_relative(base, path, flags);
    return file;
}

bool vfs_stat(const char *path, file_stat_t *restrict stat)
{
    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(FD_CWD);
    dentry_t *file = dentry_resolve(base, root_dentry, path, RESOLVE_FILE);
    if (file == NULL)
        return false;

    *stat = file->inode->stat;
    return true;
}

size_t vfs_readlink(const char *path, char *buf, size_t size)
{
    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(FD_CWD);
    dentry_t *dentry = dentry_resolve(base, root_dentry, path, RESOLVE_FILE);
    if (dentry == NULL)
        return 0;

    if (dentry->inode->stat.type != FILE_TYPE_SYMLINK)
        return 0;

    const size_t len = dentry->inode->ops->readlink(dentry, buf, size);
    if (len >= size) // buffer too small
        return 0;

    return len;
}
