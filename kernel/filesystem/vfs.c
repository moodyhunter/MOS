// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/assert.h"
#include "mos/device/timer.h"
#include "mos/filesystem/inode.h"
#include "mos/filesystem/mount.h"
#include "mos/filesystem/page_cache.h"
#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/mm/mm.h"
#include "mos/mm/mmstat.h"
#include "mos/mm/physical/pmm.h"
#include "mos/mm/slab_autoinit.h"

#include <dirent.h>
#include <errno.h>
#include <mos/filesystem/dentry.h>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.h>
#include <mos/filesystem/vfs_types.h>
#include <mos/io/io.h>
#include <mos/lib/structures/list.h>
#include <mos/lib/structures/tree.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mos_global.h>
#include <mos/platform/platform.h>
#include <mos/syslog/printk.h>
#include <mos/tasks/process.h>
#include <mos/types.h>
#include <mos_stdlib.h>
#include <mos_string.h>

static list_head vfs_fs_list = LIST_HEAD_INIT(vfs_fs_list); // filesystem_t
static spinlock_t vfs_fs_list_lock = SPINLOCK_INIT;

dentry_t *root_dentry = NULL;

slab_t *superblock_cache = NULL, *mount_cache = NULL, *file_cache = NULL;

SLAB_AUTOINIT("superblock", superblock_cache, superblock_t);
SLAB_AUTOINIT("mount", mount_cache, mount_t);
SLAB_AUTOINIT("file", file_cache, file_t);

static long do_pagecache_flush(file_t *file, off_t pgoff, size_t npages)
{
    pr_dinfo2(vfs, "vfs: flushing page cache for file %pio", (void *) &file->io);

    mutex_acquire(&file->dentry->inode->cache.lock);
    long ret = 0;
    if (pgoff == 0 && npages == (size_t) -1)
        ret = pagecache_flush_or_drop_all(&file->dentry->inode->cache, false);
    else
        ret = pagecache_flush_or_drop(&file->dentry->inode->cache, pgoff, npages, false);

    mutex_release(&file->dentry->inode->cache.lock);
    return ret;
}

static long do_sync_inode(file_t *file)
{
    const superblock_ops_t *ops = file->dentry->inode->superblock->ops;
    if (ops && ops->sync_inode)
        return ops->sync_inode(file->dentry->inode);

    return 0;
}

// BEGIN: filesystem's io_t operations
static void vfs_io_ops_close(io_t *io)
{
    file_t *file = container_of(io, file_t, io);
    if (io->type == IO_FILE && io->flags & IO_WRITABLE) // only flush if the file is writable
    {
        do_pagecache_flush(file, 0, (off_t) -1);
        do_sync_inode(file);
    }

    dentry_unref(file->dentry);

    if (io->type == IO_FILE)
    {
        const file_ops_t *file_ops = file_get_ops(file);
        if (file_ops)
        {
            if (file_ops->release)
                file_ops->release(file);
        }
    }

    kfree(file);
}

static void vfs_io_ops_close_dir(io_t *io)
{
    file_t *file = container_of(io, file_t, io);

    if (file->private_data)
    {
        vfs_listdir_state_t *state = file->private_data;
        list_foreach(vfs_listdir_entry_t, entry, state->entries)
        {
            list_remove(entry);
            kfree(entry->name);
            kfree(entry);
        }

        kfree(state);
        file->private_data = NULL;
    }

    vfs_io_ops_close(io); // close the file
}

static size_t vfs_io_ops_read(io_t *io, void *buf, size_t count)
{
    file_t *file = container_of(io, file_t, io);
    const file_ops_t *const file_ops = file_get_ops(file);
    if (!file_ops || !file_ops->read)
        return 0;

    spinlock_acquire(&file->offset_lock);
    size_t ret = file_ops->read(file, buf, count, file->offset);
    if (IS_ERR_VALUE(ret))
        ; // do nothing
    else if (ret != (size_t) -1)
        file->offset += ret;
    spinlock_release(&file->offset_lock);

    return ret;
}

static size_t vfs_io_ops_write(io_t *io, const void *buf, size_t count)
{
    file_t *file = container_of(io, file_t, io);
    const file_ops_t *const file_ops = file_get_ops(file);
    if (!file_ops || !file_ops->write)
        return 0;

    spinlock_acquire(&file->offset_lock);
    size_t ret = file_ops->write(file, buf, count, file->offset);
    if (!IS_ERR_VALUE(ret))
        file->offset += ret;
    spinlock_release(&file->offset_lock);
    return ret;
}

static off_t vfs_io_ops_seek(io_t *io, off_t offset, io_seek_whence_t whence)
{
    file_t *file = container_of(io, file_t, io);

    const file_ops_t *const ops = file_get_ops(file);
    if (ops->seek)
        return ops->seek(file, offset, whence); // use the filesystem's lseek if it exists

    spinlock_acquire(&file->offset_lock);

    switch (whence)
    {
        case IO_SEEK_SET:
        {
            file->offset = MAX(offset, 0);
            break;
        }
        case IO_SEEK_CURRENT:
        {
            off_t new_offset = file->offset + offset;
            new_offset = MAX(new_offset, 0);
            file->offset = new_offset;
            break;
        }
        case IO_SEEK_END:
        {
            off_t new_offset = file->dentry->inode->size + offset;
            new_offset = MAX(new_offset, 0);
            file->offset = new_offset;
            break;
        }
        case IO_SEEK_DATA: mos_warn("vfs: IO_SEEK_DATA is not supported"); break;
        case IO_SEEK_HOLE: mos_warn("vfs: IO_SEEK_HOLE is not supported"); break;
    };

    spinlock_release(&file->offset_lock);
    return file->offset;
}

static vmfault_result_t vfs_fault_handler(vmap_t *vmap, ptr_t fault_addr, pagefault_t *info)
{
    MOS_ASSERT(vmap->io);
    file_t *file = container_of(vmap->io, file_t, io);
    const size_t fault_pgoffset = (vmap->io_offset + ALIGN_DOWN_TO_PAGE(fault_addr) - vmap->vaddr) / MOS_PAGE_SIZE;

    mutex_acquire(&file->dentry->inode->cache.lock); // lock the inode cache
    phyframe_t *const pagecache_page = pagecache_get_page_for_read(&file->dentry->inode->cache, fault_pgoffset);
    mutex_release(&file->dentry->inode->cache.lock);

    if (IS_ERR(pagecache_page))
        return VMFAULT_CANNOT_HANDLE;

    // ! mm subsystem has verified that this vmap can be written to, but in the page table it's marked as read-only
    // * currently, only CoW pages have this property, we treat this as a CoW page
    if (info->is_present && info->is_write)
    {
        if (pagecache_page == info->faulting_page)
            vmap_stat_dec(vmap, pagecache); // the faulting page is a pagecache page
        else
            vmap_stat_dec(vmap, cow); // the faulting page is a COW page
        vmap_stat_inc(vmap, regular);
        return mm_resolve_cow_fault(vmap, fault_addr, info); // resolve by copying data page into prevate page
    }

    info->backing_page = pagecache_page;
    if (vmap->type == VMAP_TYPE_PRIVATE)
    {
        if (info->is_write)
        {
            vmap_stat_inc(vmap, regular);
            // present pages are handled above
            MOS_ASSERT(!info->is_present);
            return VMFAULT_COPY_BACKING_PAGE; // copy and (also) map the backing page
        }
        else
        {
            vmap_stat_inc(vmap, pagecache);
            vmap_stat_inc(vmap, cow);
            return VMFAULT_MAP_BACKING_PAGE_RO;
        }
    }
    else
    {
        vmap_stat_inc(vmap, pagecache);
        vmap_stat_inc(vmap, regular);
        return VMFAULT_MAP_BACKING_PAGE;
    }
}

static bool vfs_io_ops_mmap(io_t *io, vmap_t *vmap, off_t offset)
{
    file_t *file = container_of(io, file_t, io);
    const file_ops_t *const file_ops = file_get_ops(file);

    MOS_ASSERT(!vmap->on_fault); // there should be no fault handler set
    vmap->on_fault = vfs_fault_handler;

    if (file_ops->mmap)
        return file_ops->mmap(file, vmap, offset);

    return true;
}

static bool vfs_io_ops_munmap(io_t *io, vmap_t *vmap, bool *unmapped)
{
    file_t *file = container_of(io, file_t, io);
    const file_ops_t *const file_ops = file_get_ops(file);

    if (file_ops->munmap)
        return file_ops->munmap(file, vmap, unmapped);

    return true;
}

static void vfs_io_ops_getname(const io_t *io, char *buf, size_t size)
{
    const file_t *file = container_of(io, file_t, io);
    dentry_path(file->dentry, root_dentry, buf, size);
}

static const io_op_t file_io_ops = {
    .read = vfs_io_ops_read,
    .write = vfs_io_ops_write,
    .close = vfs_io_ops_close,
    .seek = vfs_io_ops_seek,
    .mmap = vfs_io_ops_mmap,
    .munmap = vfs_io_ops_munmap,
    .get_name = vfs_io_ops_getname,
};

static const io_op_t dir_io_ops = {
    .read = vfs_list_dir,
    .close = vfs_io_ops_close_dir,
    .get_name = vfs_io_ops_getname,
};

// END: filesystem's io_t operations

static void vfs_flusher_entry(void *arg)
{
    MOS_UNUSED(arg);
    while (true)
    {
        timer_msleep(10 * 1000);
        // pagecache_flush_all();
    }
}

static void vfs_flusher_init(void)
{
    // kthread_create(vfs_flusher_entry, NULL, "vfs_flusher");
}
MOS_INIT(KTHREAD, vfs_flusher_init);

static void vfs_copy_stat(file_stat_t *statbuf, inode_t *inode)
{
    statbuf->ino = inode->ino;
    statbuf->type = inode->type;
    statbuf->perm = inode->perm;
    statbuf->size = inode->size;
    statbuf->uid = inode->uid;
    statbuf->gid = inode->gid;
    statbuf->sticky = inode->sticky;
    statbuf->suid = inode->suid;
    statbuf->sgid = inode->sgid;
    statbuf->nlinks = inode->nlinks;
    statbuf->accessed = inode->accessed;
    statbuf->modified = inode->modified;
    statbuf->created = inode->created;
}

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
    MOS_ASSERT(file_dentry && file_dentry->inode);
    const file_perm_t file_perm = file_dentry->inode->perm;

    // TODO: we are treating all users as root for now, only checks for execute permission
    MOS_UNUSED(open);
    MOS_UNUSED(read);
    MOS_UNUSED(create);
    MOS_UNUSED(write);

    if (execute && !(file_perm & PERM_EXEC))
        return false; // execute permission denied

    return true;
}

static file_t *vfs_do_open(dentry_t *base, const char *path, open_flags flags)
{
    if (base == NULL)
        return NULL;

    const bool may_create = flags & OPEN_CREATE;
    const bool read = flags & OPEN_READ;
    const bool write = flags & OPEN_WRITE;
    const bool exec = flags & OPEN_EXECUTE;
    const bool no_follow = flags & OPEN_NO_FOLLOW;
    const bool expect_dir = flags & OPEN_DIR;
    const bool truncate = flags & OPEN_TRUNCATE;

    lastseg_resolve_flags_t resolve_flags = RESOLVE_EXPECT_FILE |                                            //
                                            (no_follow ? RESOLVE_SYMLINK_NOFOLLOW : 0) |                     //
                                            (may_create ? RESOLVE_EXPECT_ANY_EXIST : RESOLVE_EXPECT_EXIST) | //
                                            (expect_dir ? RESOLVE_EXPECT_DIR : 0);
    dentry_t *entry = dentry_resolve(base, root_dentry, path, resolve_flags);
    if (IS_ERR(entry))
    {
        pr_dinfo2(vfs, "failed to resolve '%s': create=%d, r=%d, x=%d, nofollow=%d, dir=%d, truncate=%d", path, may_create, read, exec, no_follow, expect_dir, truncate);
        return ERR(entry);
    }

    bool created = false;

    if (may_create && entry->inode == NULL)
    {
        dentry_t *parent = dentry_parent(entry);
        if (!parent->inode->ops->newfile)
        {
            dentry_unref(entry);
            return ERR_PTR(-EROFS);
        }

        if (!parent->inode->ops->newfile(parent->inode, entry, FILE_TYPE_REGULAR, 0666))
        {
            dentry_unref(entry);
            return ERR_PTR(-EIO); // failed to create file
        }

        created = true;
    }

    if (!vfs_verify_permissions(entry, true, read, may_create, exec, write))
    {
        dentry_unref(entry);
        return ERR_PTR(-EACCES);
    }

    file_t *file = vfs_do_open_dentry(entry, created, read, write, exec, truncate);
    if (IS_ERR(file))
    {
        kfree(file);
        dentry_unref(entry);
        return ERR(file);
    }

    return file;
}

// public functions
file_t *vfs_do_open_dentry(dentry_t *entry, bool created, bool read, bool write, bool exec, bool truncate)
{
    MOS_ASSERT(entry->inode);
    MOS_UNUSED(truncate);

    file_t *file = kmalloc(file_cache);
    file->dentry = entry;

    io_flags_t io_flags = IO_SEEKABLE;

    if (read)
        io_flags |= IO_READABLE;

    if (write)
        io_flags |= IO_WRITABLE;

    if (exec)
        io_flags |= IO_EXECUTABLE;

    // only regular files are mmapable
    if (entry->inode->type == FILE_TYPE_REGULAR)
        io_flags |= IO_MMAPABLE;

    if (file->dentry->inode->type == FILE_TYPE_DIRECTORY)
        io_init(&file->io, IO_DIR, (io_flags | IO_READABLE) & ~IO_SEEKABLE, &dir_io_ops);
    else
        io_init(&file->io, IO_FILE, io_flags, &file_io_ops);

    const file_ops_t *ops = file_get_ops(file);
    if (ops && ops->open)
    {
        bool opened = ops->open(file->dentry->inode, file, created);
        if (!opened)
            return ERR_PTR(-ENOTSUP);
    }

    return file;
}

void vfs_register_filesystem(filesystem_t *fs)
{
    if (vfs_find_filesystem(fs->name))
        mos_panic("filesystem '%s' already registered", fs->name);

    MOS_ASSERT(list_is_empty(list_node(fs)));

    spinlock_acquire(&vfs_fs_list_lock);
    list_node_append(&vfs_fs_list, list_node(fs));
    spinlock_release(&vfs_fs_list_lock);

    pr_dinfo2(vfs, "filesystem '%s' registered", fs->name);
}

long vfs_mount(const char *device, const char *path, const char *fs, const char *options)
{
    filesystem_t *real_fs = vfs_find_filesystem(fs);
    if (unlikely(real_fs == NULL))
    {
        mos_warn("filesystem '%s' not found", fs);
        return -EINVAL;
    }

    MOS_ASSERT_X(real_fs->mount, "filesystem '%s' does not support mounting", real_fs->name);

    if (unlikely(strcmp(path, "/") == 0))
    {
        // special case: mount root filesystem
        if (root_dentry)
        {
            pr_warn("root filesystem is already mounted");
            return -EBUSY;
        }
        pr_dinfo2(vfs, "mounting root filesystem '%s'...", fs);
        root_dentry = real_fs->mount(real_fs, device, options);
        if (root_dentry == NULL)
        {
            mos_warn("failed to mount root filesystem");
            return -EIO;
        }
        pr_dinfo2(vfs, "root filesystem mounted, dentry=%p", (void *) root_dentry);

        MOS_ASSERT(root_dentry->name == NULL);
        bool mounted = dentry_mount(root_dentry, root_dentry, real_fs);
        MOS_ASSERT(mounted);

        return 0;
    }

    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(AT_FDCWD);
    dentry_t *mountpoint = dentry_resolve(base, root_dentry, path, RESOLVE_EXPECT_DIR | RESOLVE_EXPECT_EXIST);
    if (IS_ERR(mountpoint))
        return PTR_ERR(mountpoint);

    if (mountpoint->is_mountpoint)
    {
        // we don't support overlaying filesystems yet
        mos_warn("mount point is already mounted");
        dentry_unref(mountpoint);
        return -ENOTSUP;
    }

    // when mounting:
    // mounted_root will have a reference of 1
    // the mount_point will have its reference incremented by 1
    dentry_t *mounted_root = real_fs->mount(real_fs, device, options);
    if (IS_ERR(mounted_root))
    {
        mos_warn("failed to mount filesystem");
        return PTR_ERR(mounted_root);
    }

    const bool mounted = dentry_mount(mountpoint, mounted_root, real_fs);
    if (unlikely(!mounted))
    {
        mos_warn("failed to mount filesystem");
        return -EIO;
    }

    MOS_ASSERT_X(mountpoint->refcount == mounted_root->refcount, "mountpoint refcount=%zu, mounted_root refcount=%zu", mountpoint->refcount, mounted_root->refcount);
    pr_dinfo2(vfs, "mounted filesystem '%s' on '%s'", fs, path);
    return 0;
}

long vfs_unmount(const char *path)
{
    dentry_t *mounted_root = dentry_resolve(root_dentry, root_dentry, path, RESOLVE_EXPECT_DIR | RESOLVE_EXPECT_EXIST);
    if (IS_ERR(mounted_root))
        return PTR_ERR(mounted_root);

    // the mounted root itself holds a ref, and the caller of this function
    if (mounted_root->refcount != 2)
    {
        dentry_check_refstat(mounted_root);
        mos_warn("refcount is not as expected");
        return -EBUSY;
    }

    dentry_unref(mounted_root); // release the reference held by this function

    // unmounting root filesystem
    dentry_t *mountpoint = dentry_unmount(mounted_root);
    if (!mountpoint)
    {
        mos_warn("failed to unmount filesystem");
        return -EIO;
    }

    MOS_ASSERT(mounted_root->refcount == mountpoint->refcount && mountpoint->refcount == 1);
    if (mounted_root->superblock->fs->unmount)
        mounted_root->superblock->fs->unmount(mounted_root->superblock->fs, mounted_root);
    else
        MOS_ASSERT(dentry_unref_one_norelease(mounted_root));
    MOS_ASSERT_X(mounted_root->refcount == 0, "fs->umount should release the last reference to the mounted root");

    if (mounted_root == root_dentry)
    {
        pr_info2("unmounted root filesystem");
        root_dentry = NULL;
        return 0;
    }

    dentry_unref(mountpoint);
    return 0;
}

file_t *vfs_openat(int fd, const char *path, open_flags flags)
{
    pr_dinfo2(vfs, "vfs_openat(fd=%d, path='%s', flags=%x)", fd, path, flags);
    dentry_t *basedir = path_is_absolute(path) ? root_dentry : dentry_from_fd(fd);
    if (IS_ERR(basedir))
        return ERR(basedir);
    file_t *file = vfs_do_open(basedir, path, flags);
    return file;
}

long vfs_fstatat(fd_t fd, const char *path, file_stat_t *restrict statbuf, fstatat_flags flags)
{
    if (flags & FSTATAT_FILE)
    {
        pr_dinfo2(vfs, "vfs_fstatat(fd=%d, path='%p', stat=%p, flags=%x)", fd, (void *) path, (void *) statbuf, flags);
        io_t *io = process_get_fd(current_process, fd);
        if (!(io_valid(io) && (io->type == IO_FILE || io->type == IO_DIR)))
            return -EBADF; // io is closed, or is not a file or directory

        file_t *file = container_of(io, file_t, io);
        MOS_ASSERT(file);
        if (statbuf)
            vfs_copy_stat(statbuf, file->dentry->inode);

        return 0;
    }

    pr_dinfo2(vfs, "vfs_fstatat(fd=%d, path='%s', stat=%p, flags=%x)", fd, path, (void *) statbuf, flags);
    dentry_t *basedir = path_is_absolute(path) ? root_dentry : dentry_from_fd(fd);
    if (IS_ERR(basedir))
        return PTR_ERR(basedir);
    lastseg_resolve_flags_t resolve_flags = RESOLVE_EXPECT_ANY_TYPE | RESOLVE_EXPECT_EXIST;
    if (flags & FSTATAT_NOFOLLOW)
        resolve_flags |= RESOLVE_SYMLINK_NOFOLLOW;

    dentry_t *dentry = dentry_resolve(basedir, root_dentry, path, resolve_flags);
    if (IS_ERR(dentry))
        return PTR_ERR(dentry);

    if (statbuf)
        vfs_copy_stat(statbuf, dentry->inode);
    dentry_unref(dentry);
    return 0;
}

size_t vfs_readlinkat(fd_t dirfd, const char *path, char *buf, size_t size)
{
    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(dirfd);
    dentry_t *dentry = dentry_resolve(base, root_dentry, path, RESOLVE_SYMLINK_NOFOLLOW | RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_FILE);
    if (IS_ERR(dentry))
        return PTR_ERR(dentry);

    if (dentry->inode->type != FILE_TYPE_SYMLINK)
    {
        dentry_unref(dentry);
        return -EINVAL;
    }

    const size_t len = dentry->inode->ops->readlink(dentry, buf, size);

    dentry_unref(dentry);

    if (len >= size) // buffer too small
        return -ENAMETOOLONG;

    return len;
}

long vfs_symlink(const char *path, const char *target)
{
    pr_dinfo2(vfs, "vfs_symlink(path='%s', target='%s')", path, target);
    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(AT_FDCWD);
    dentry_t *dentry = dentry_resolve(base, root_dentry, path, RESOLVE_EXPECT_NONEXIST);
    if (IS_ERR(dentry))
        return PTR_ERR(dentry);

    dentry_t *parent_dir = dentry_parent(dentry);
    const bool created = parent_dir->inode->ops->symlink(parent_dir->inode, dentry, target);

    if (!created)
        mos_warn("failed to create symlink '%s'", path);

    dentry_unref(dentry);
    return created ? 0 : -EIO;
}

long vfs_mkdir(const char *path)
{
    pr_dinfo2(vfs, "vfs_mkdir('%s')", path);
    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(AT_FDCWD);
    dentry_t *dentry = dentry_resolve(base, root_dentry, path, RESOLVE_EXPECT_NONEXIST);
    if (IS_ERR(dentry))
        return PTR_ERR(dentry);

    dentry_t *parent_dir = dentry_parent(dentry);
    if (parent_dir->inode == NULL || parent_dir->inode->ops == NULL || parent_dir->inode->ops->mkdir == NULL)
    {
        dentry_unref(dentry);
        return false;
    }

    // TODO: use umask or something else
    const bool created = parent_dir->inode->ops->mkdir(parent_dir->inode, dentry, parent_dir->inode->perm);

    if (!created)
        mos_warn("failed to create directory '%s'", path);

    dentry_unref(dentry);
    return created ? 0 : -EIO;
}

long vfs_rmdir(const char *path)
{
    pr_dinfo2(vfs, "vfs_rmdir('%s')", path);
    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(AT_FDCWD);
    dentry_t *dentry = dentry_resolve(base, root_dentry, path, RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_DIR);
    if (IS_ERR(dentry))
        return PTR_ERR(dentry);

    dentry_t *parent_dir = dentry_parent(dentry);
    if (parent_dir->inode == NULL || parent_dir->inode->ops == NULL || parent_dir->inode->ops->rmdir == NULL)
    {
        dentry_unref(dentry);
        return -ENOTSUP;
    }

    const bool removed = parent_dir->inode->ops->rmdir(parent_dir->inode, dentry);

    if (!removed)
        mos_warn("failed to remove directory '%s'", path);

    dentry_unref(dentry);
    return removed ? 0 : -EIO;
}

size_t vfs_list_dir(io_t *io, void *user_buf, size_t user_size)
{
    pr_dinfo2(vfs, "vfs_list_dir(io=%p, buf=%p, size=%zu)", (void *) io, (void *) user_buf, user_size);
    file_t *file = container_of(io, file_t, io);
    if (unlikely(file->dentry->inode->type != FILE_TYPE_DIRECTORY))
    {
        mos_warn("not a directory");
        return 0;
    }

    if (file->private_data == NULL)
    {
        vfs_listdir_state_t *const state = file->private_data = kmalloc(sizeof(vfs_listdir_state_t));
        linked_list_init(&state->entries);
        state->n_count = state->read_offset = 0;
        vfs_populate_listdir_buf(file->dentry, state);
    }

    vfs_listdir_state_t *const state = file->private_data;

    if (state->read_offset >= state->n_count)
        return 0; // no more entries

    size_t bytes_copied = 0;
    size_t i = 0;
    list_foreach(vfs_listdir_entry_t, entry, state->entries)
    {
        if (i++ < state->read_offset)
            continue; // skip the entries we have already read

        if (state->read_offset >= state->n_count)
            break;

        const size_t entry_size = sizeof(ino_t) + sizeof(off_t) + sizeof(short) + sizeof(char) + entry->name_len + 1; // +1 for the null terminator
        if (bytes_copied + entry_size > user_size)
            break;

        struct dirent *dirent = (struct dirent *) (((char *) user_buf) + bytes_copied);
        dirent->d_ino = entry->ino;
        dirent->d_type = entry->type;
        dirent->d_reclen = entry_size;
        dirent->d_off = entry_size - 1;
        memcpy(dirent->d_name, entry->name, entry->name_len);
        dirent->d_name[entry->name_len] = '\0';
        bytes_copied += entry_size;
        state->read_offset++;
    }

    return bytes_copied;
}

long vfs_chdirat(fd_t dirfd, const char *path)
{
    pr_dinfo2(vfs, "vfs_chdir('%s')", path);
    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(dirfd);
    dentry_t *dentry = dentry_resolve(base, root_dentry, path, RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_DIR);
    if (IS_ERR(dentry))
        return PTR_ERR(dentry);

    dentry_t *old_cwd = dentry_from_fd(AT_FDCWD);
    if (old_cwd)
        dentry_unref(old_cwd);

    current_process->working_directory = dentry;
    return 0;
}

ssize_t vfs_getcwd(char *buf, size_t size)
{
    dentry_t *cwd = dentry_from_fd(AT_FDCWD);
    if (IS_ERR(cwd))
        return PTR_ERR(cwd);

    return dentry_path(cwd, root_dentry, buf, size);
}

long vfs_fchmodat(fd_t fd, const char *path, int perm, int flags)
{
    pr_dinfo2(vfs, "vfs_fchmodat(fd=%d, path='%s', perm=%o, flags=%x)", fd, path, perm, flags);
    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(fd);
    dentry_t *dentry = dentry_resolve(base, root_dentry, path, RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_ANY_TYPE);
    if (IS_ERR(dentry))
        return PTR_ERR(dentry);

    // TODO: check if the underlying filesystem supports chmod, and is not read-only
    dentry->inode->perm = perm;
    dentry_unref(dentry);
    return 0;
}

long vfs_unlinkat(fd_t dirfd, const char *path)
{
    pr_dinfo2(vfs, "vfs_unlinkat(dirfd=%d, path='%s')", dirfd, path);
    dentry_t *base = path_is_absolute(path) ? root_dentry : dentry_from_fd(dirfd);
    dentry_t *dentry = dentry_resolve(base, root_dentry, path, RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_FILE | RESOLVE_SYMLINK_NOFOLLOW);
    if (IS_ERR(dentry))
        return PTR_ERR(dentry);

    dentry_t *parent_dir = dentry_parent(dentry);
    if (parent_dir->inode == NULL || parent_dir->inode->ops == NULL || parent_dir->inode->ops->unlink == NULL)
    {
        dentry_unref(dentry);
        return -ENOTSUP;
    }

    if (!inode_unlink(parent_dir->inode, dentry))
    {
        dentry_unref(dentry);
        return -EIO;
    }

    dentry_unref(dentry); // it won't release dentry because dentry->inode is still valid
    dentry_detach(dentry);
    dentry_try_release(dentry);
    return 0;
}

long vfs_fsync(io_t *io, bool sync_metadata, off_t start, off_t end)
{
    pr_dinfo2(vfs, "vfs_fsync(io=%p, sync_metadata=%d, start=%ld, end=%ld)", (void *) io, sync_metadata, start, end);
    file_t *file = container_of(io, file_t, io);

    const off_t nbytes = end - start;
    const off_t npages = ALIGN_UP_TO_PAGE(nbytes) / MOS_PAGE_SIZE;
    const off_t pgoffset = start / MOS_PAGE_SIZE;

    long ret = do_pagecache_flush(file, pgoffset, npages);
    if (ret < 0)
        return ret;

    if (sync_metadata)
    {
        ret = do_sync_inode(file);
        if (ret < 0)
            return ret;
    }

    return ret;
}

// ! sysfs support

static bool vfs_sysfs_filesystems(sysfs_file_t *f)
{
    list_foreach(filesystem_t, fs, vfs_fs_list)
    {
        sysfs_printf(f, "%s\n", fs->name);
    }

    return true;
}

static bool vfs_sysfs_mountpoints(sysfs_file_t *f)
{
    char pathbuf[MOS_PATH_MAX_LENGTH];
    list_foreach(mount_t, mp, vfs_mountpoint_list)
    {
        dentry_path(mp->mountpoint, root_dentry, pathbuf, sizeof(pathbuf));
        sysfs_printf(f, "%-20s %-10s\n", pathbuf, mp->fs->name);
    }

    return true;
}

static void vfs_sysfs_dentry_stats_stat_receiver(int depth, const dentry_t *dentry, bool mountroot, void *data)
{
    sysfs_file_t *file = data;
    sysfs_printf(file, "%*s%s: refcount=%zu%s\n",                                             //
                 depth * 4,                                                                   //
                 "",                                                                          //
                 dentry_name(dentry),                                                         //
                 dentry->refcount,                                                            //
                 mountroot ? " (mount root)" : (dentry->is_mountpoint ? " (mountpoint)" : "") //
    );
}

static bool vfs_sysfs_dentry_stats(sysfs_file_t *f)
{
    dentry_dump_refstat(root_dentry, vfs_sysfs_dentry_stats_stat_receiver, f);
    return true;
}

static sysfs_item_t vfs_sysfs_items[] = {
    SYSFS_RO_ITEM("filesystems", vfs_sysfs_filesystems),
    SYSFS_RO_ITEM("mount", vfs_sysfs_mountpoints),
    SYSFS_RO_ITEM("dentry_stats", vfs_sysfs_dentry_stats),
};

SYSFS_AUTOREGISTER(vfs, vfs_sysfs_items);
