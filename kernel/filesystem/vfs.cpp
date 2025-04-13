// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/assert.hpp"
#include "mos/device/timer.hpp"
#include "mos/filesystem/inode.hpp"
#include "mos/filesystem/mount.hpp"
#include "mos/filesystem/page_cache.hpp"
#include "mos/filesystem/sysfs/sysfs.hpp"
#include "mos/filesystem/sysfs/sysfs_autoinit.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/mmstat.hpp"

#include <algorithm>
#include <dirent.h>
#include <errno.h>
#include <mos/filesystem/dentry.hpp>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.hpp>
#include <mos/filesystem/vfs_types.hpp>
#include <mos/io/io.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/lib/structures/tree.hpp>
#include <mos/lib/sync/spinlock.hpp>
#include <mos/mos_global.h>
#include <mos/platform/platform.hpp>
#include <mos/syslog/printk.hpp>
#include <mos/tasks/process.hpp>
#include <mos/types.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

static list_head vfs_fs_list; // filesystem_t
static spinlock_t vfs_fs_list_lock;

dentry_t *root_dentry = NULL;

static long do_pagecache_flush(FsBaseFile *file, off_t pgoff, size_t npages)
{
    dInfo2<vfs> << "vfs: flushing page cache for file " << (void *) file;

    mutex_acquire(&file->dentry->inode->cache.lock);
    long ret = 0;
    if (pgoff == 0 && npages == (size_t) -1)
        ret = pagecache_flush_or_drop_all(&file->dentry->inode->cache, false);
    else
        ret = pagecache_flush_or_drop(&file->dentry->inode->cache, pgoff, npages, false);

    mutex_release(&file->dentry->inode->cache.lock);
    return ret;
}

static long do_sync_inode(FsBaseFile *file)
{
    const superblock_ops_t *ops = file->dentry->inode->superblock->ops;
    if (ops && ops->sync_inode)
        return ops->sync_inode(file->dentry->inode);

    return 0;
}

// BEGIN: filesystem's IO operations
void FsFile::on_closed()
{
    if (io_type == IO_FILE && io_flags.test(IO_WRITABLE)) // only flush if the file is writable
    {
        do_pagecache_flush(this, 0, (off_t) -1);
        do_sync_inode(this);
    }

    dentry_unref(this->dentry);

    if (io_type == IO_FILE)
    {
        const file_ops_t *file_ops = get_ops();
        if (file_ops)
        {
            if (file_ops->release)
                file_ops->release(this);
        }
    }

    delete this;
}

size_t FsDir::on_read(void *buf, size_t bufSize)
{
    return vfs_list_dir(this, buf, bufSize);
}

void FsDir::on_closed()
{
    if (this->private_data)
    {
        vfs_listdir_state_t *state = static_cast<vfs_listdir_state_t *>(this->private_data);
        list_foreach(vfs_listdir_entry_t, entry, state->entries)
        {
            list_remove(entry);
            delete entry;
        }

        delete state;
        this->private_data = NULL;
    }

    dentry_unref(this->dentry);

    if (io_type == IO_FILE)
    {
        const file_ops_t *file_ops = get_ops();
        if (file_ops)
        {
            if (file_ops->release)
                file_ops->release(this);
        }
    }

    delete this;
}

size_t FsFile::on_read(void *buf, size_t count)
{
    const file_ops_t *const file_ops = get_ops();
    if (!file_ops || !file_ops->read)
        return 0;

    spinlock_acquire(&offset_lock);
    size_t ret = file_ops->read(this, buf, count, this->offset);
    if (IS_ERR_VALUE(ret))
        ; // do nothing
    else if (ret != (size_t) -1)
        this->offset += ret;
    spinlock_release(&offset_lock);
    return ret;
}

size_t FsFile::on_write(const void *buf, size_t count)
{
    const file_ops_t *const file_ops = get_ops();
    if (!file_ops || !file_ops->write)
        return 0;

    spinlock_acquire(&offset_lock);
    size_t ret = file_ops->write(this, buf, count, this->offset);
    if (!IS_ERR_VALUE(ret))
        this->offset += ret;
    spinlock_release(&offset_lock);
    return ret;
}

off_t FsFile::on_seek(off_t offset, io_seek_whence_t whence)
{
    const file_ops_t *const ops = get_ops();
    if (ops->seek)
        return ops->seek(this, offset, whence); // use the filesystem's lseek if it exists

    spinlock_acquire(&offset_lock);

    switch (whence)
    {
        case IO_SEEK_SET:
        {
            this->offset = std::max(offset, 0l);
            break;
        }
        case IO_SEEK_CURRENT:
        {
            off_t new_offset = this->offset + offset;
            new_offset = std::max(new_offset, 0l);
            this->offset = new_offset;
            break;
        }
        case IO_SEEK_END:
        {
            off_t new_offset = this->dentry->inode->size + offset;
            new_offset = std::max(new_offset, 0l);
            this->offset = new_offset;
            break;
        }
        case IO_SEEK_DATA: dInfo2<vfs> << "vfs: IO_SEEK_DATA is not supported"; break;
        case IO_SEEK_HOLE: dInfo2<vfs> << "vfs: IO_SEEK_HOLE is not supported"; break;
    };

    spinlock_release(&offset_lock);
    return this->offset;
}

static vmfault_result_t vfs_fault_handler(vmap_t *vmap, ptr_t fault_addr, pagefault_t *info)
{
    MOS_ASSERT(vmap->io);
    FsBaseFile *file = static_cast<FsBaseFile *>(vmap->io);
    const size_t fault_pgoffset = (vmap->io_offset + ALIGN_DOWN_TO_PAGE(fault_addr) - vmap->vaddr) / MOS_PAGE_SIZE;

    mutex_acquire(&file->dentry->inode->cache.lock); // lock the inode cache
    auto pagecache_page = pagecache_get_page_for_read(&file->dentry->inode->cache, fault_pgoffset);
    mutex_release(&file->dentry->inode->cache.lock);

    if (pagecache_page.isErr())
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

    info->backing_page = pagecache_page.get();
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

bool FsFile::on_mmap(vmap_t *vmap, off_t offset)
{
    const file_ops_t *const file_ops = get_ops();

    MOS_ASSERT(!vmap->on_fault); // there should be no fault handler set
    vmap->on_fault = vfs_fault_handler;

    if (file_ops->mmap)
        return file_ops->mmap(this, vmap, offset);

    return true;
}

bool FsFile::on_munmap(vmap_t *vmap, bool *unmapped)
{
    const file_ops_t *const file_ops = get_ops();

    if (file_ops->munmap)
        return file_ops->munmap(this, vmap, unmapped);

    return true;
}

// static const io_op_t file_io_ops = {
//     .read = vfs_io_ops_read,
//     .write = vfs_io_ops_write,
//     .close = vfs_io_ops_close,
//     .seek = vfs_io_ops_seek,
//     .mmap = vfs_io_ops_mmap,
//     .munmap = vfs_io_ops_munmap,
// };

// static const io_op_t dir_io_ops = {
//     .read = vfs_list_dir,
//     .close = vfs_io_ops_close_dir,
// };

// END: filesystem's IO operations

static __used void vfs_flusher_entry(void *arg)
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

static filesystem_t *vfs_find_filesystem(mos::string_view name)
{
    SpinLocker lock(&vfs_fs_list_lock);
    list_foreach(filesystem_t, fs, vfs_fs_list)
    {
        if (fs->name == name)
            return fs;
    }

    return nullptr;
}

static bool vfs_verify_permissions(dentry_t &file_dentry, bool open, bool read, bool create, bool execute, bool write)
{
    MOS_ASSERT(file_dentry.inode);
    const file_perm_t file_perm = file_dentry.inode->perm;

    // TODO: we are treating all users as root for now, only checks for execute permission
    MOS_UNUSED(open);
    MOS_UNUSED(read);
    MOS_UNUSED(create);
    MOS_UNUSED(write);

    if (execute && !(file_perm & PERM_EXEC))
        return false; // execute permission denied

    return true;
}

static PtrResult<FsBaseFile> vfs_do_open(dentry_t *base, mos::string_view path, OpenFlags flags)
{
    if (base == NULL)
        return -EINVAL;

    const bool may_create = flags & OPEN_CREATE;
    const bool read = flags & OPEN_READ;
    const bool write = flags & OPEN_WRITE;
    const bool exec = flags & OPEN_EXECUTE;
    const bool no_follow = flags & OPEN_NO_FOLLOW;
    const bool expect_dir = flags & OPEN_DIR;
    const bool truncate = flags & OPEN_TRUNCATE;

    LastSegmentResolveFlags resolve_flags = RESOLVE_EXPECT_FILE;
    if (no_follow)
        resolve_flags |= RESOLVE_SYMLINK_NOFOLLOW;
    if (may_create)
        resolve_flags |= RESOLVE_EXPECT_ANY_EXIST;
    if (expect_dir)
        resolve_flags |= RESOLVE_EXPECT_DIR;

    auto entry = dentry_resolve(base, root_dentry, path, resolve_flags);
    if (entry.isErr())
    {
        dInfo2<vfs> << "failed to resolve '" << path << "': create=" << may_create << ", r=" << read << ", x=" << exec << ", nofollow=" << no_follow
                    << ", dir=" << expect_dir << ", truncate=" << truncate;
        return entry.getErr();
    }

    bool created = false;

    if (may_create && entry->inode == NULL)
    {
        auto parent = dentry_parent(*entry);
        if (!parent->inode->ops->newfile)
        {
            dentry_unref(entry.get());
            return -EROFS;
        }

        if (!parent->inode->ops->newfile(parent->inode, entry.get(), FILE_TYPE_REGULAR, 0666))
        {
            dentry_unref(entry.get());
            return -EIO; // failed to create file
        }

        created = true;
    }

    if (!vfs_verify_permissions(*entry, true, read, may_create, exec, write))
    {
        dentry_unref(entry.get());
        return -EACCES;
    }

    auto file = vfs_do_open_dentry(entry.get(), created, read, write, exec, truncate);
    if (file.isErr())
    {
        dentry_unref(entry.get());
        return file.getErr();
    }

    return file;
}

mos::string FsBaseFile::name() const
{
    return dentry_path(dentry, root_dentry).value_or("<unknown>");
}

// public functions
PtrResult<FsBaseFile> vfs_do_open_dentry(dentry_t *dentry, bool created, bool read, bool write, bool exec, bool truncate)
{
    MOS_ASSERT(dentry->inode);
    MOS_UNUSED(truncate);

    Flags io_flags = IO_SEEKABLE;

    if (read)
        io_flags |= IO_READABLE;

    if (write)
        io_flags |= IO_WRITABLE;

    if (exec)
        io_flags |= IO_EXECUTABLE;

    // only regular files are mmapable
    if (dentry->inode->type == FILE_TYPE_REGULAR)
        io_flags |= IO_MMAPABLE;

    FsBaseFile *file = nullptr;

    if (dentry->inode->type == FILE_TYPE_DIRECTORY)
        file = mos::create<FsDir>((io_flags | IO_READABLE).erase(IO_SEEKABLE), dentry);
    else
        file = mos::create<FsFile>(io_flags, dentry);

    const file_ops_t *ops = file->get_ops();
    if (ops && ops->open)
    {
        bool opened = ops->open(file->dentry->inode, file, created);
        if (!opened)
        {
            delete file;
            return -ENOTSUP;
        }
    }

    return file;
}

void vfs_register_filesystem(filesystem_t *fs)
{
    if (vfs_find_filesystem(fs->name))
        mos_panic("filesystem '%s' already registered", fs->name.c_str());

    MOS_ASSERT(list_is_empty(list_node(fs)));

    spinlock_acquire(&vfs_fs_list_lock);
    list_node_append(&vfs_fs_list, list_node(fs));
    spinlock_release(&vfs_fs_list_lock);

    dInfo2<vfs> << "filesystem '" << fs->name << "' registered";
}

PtrResult<void> vfs_mount(const char *device, const char *path, const char *fs, const char *options)
{
    filesystem_t *real_fs = vfs_find_filesystem(fs);
    if (unlikely(real_fs == NULL))
    {
        mos_warn("filesystem '%s' not found", fs);
        return -EINVAL;
    }

    MOS_ASSERT_X(real_fs->mount, "filesystem '%s' does not support mounting", real_fs->name.c_str());

    if (unlikely(strcmp(path, "/") == 0))
    {
        // special case: mount root filesystem
        if (root_dentry)
        {
            mWarn << "root filesystem is already mounted";
            return -EBUSY;
        }
        dInfo2<vfs> << "mounting root filesystem '" << fs << "'...";
        const auto mountResult = real_fs->mount(real_fs, device, options);
        if (mountResult.isErr())
        {
            mWarn << "failed to mount root filesystem";
            return -EIO;
        }
        else
        {
            root_dentry = mountResult.get();
        }

        dInfo2<vfs> << "root filesystem mounted, dentry=" << (void *) root_dentry;

        MOS_ASSERT(root_dentry->name.empty());
        bool mounted = dentry_mount(root_dentry, root_dentry, real_fs);
        MOS_ASSERT(mounted);

        return 0;
    }

    auto base = path_is_absolute(path) ? root_dentry : dentry_from_fd(AT_FDCWD);
    if (base.isErr())
        return base.getErr();

    auto mpRoot = dentry_resolve(base.get(), root_dentry, path, RESOLVE_EXPECT_DIR | RESOLVE_EXPECT_EXIST);
    if (mpRoot.isErr())
        return mpRoot.getErr();

    if (mpRoot->is_mountpoint)
    {
        // we don't support overlaying filesystems yet
        mWarn << "mount point is already mounted";
        dentry_unref(mpRoot.get());
        return -ENOTSUP;
    }

    // when mounting:
    // mounted_root will have a reference of 1
    // the mount_point will have its reference incremented by 1
    auto mounted_root = real_fs->mount(real_fs, device, options);
    if (mounted_root.isErr())
    {
        mWarn << "failed to mount filesystem";
        return mounted_root.getErr();
    }

    const bool mounted = dentry_mount(mpRoot.get(), mounted_root.get(), real_fs);
    if (unlikely(!mounted))
    {
        mWarn << "failed to mount filesystem";
        return -EIO;
    }

    MOS_ASSERT_X(mpRoot->refcount == mounted_root->refcount, "mountpoint refcount=%zu, mounted_root refcount=%zu", mpRoot->refcount.load(),
                 mounted_root->refcount.load());
    dInfo2<vfs> << "mounted filesystem '" << fs << "' on '" << path << "'";
    return 0;
}

long vfs_unmount(const char *path)
{
    auto mounted_root = dentry_resolve(root_dentry, root_dentry, path, RESOLVE_EXPECT_DIR | RESOLVE_EXPECT_EXIST);
    if (mounted_root.isErr())
        return mounted_root.getErr();

    // the mounted root itself holds a ref, and the caller of this function
    if (mounted_root->refcount != 2)
    {
        dentry_check_refstat(mounted_root.get());
        mWarn << "refcount is not as expected";
        return -EBUSY;
    }

    dentry_unref(mounted_root.get()); // release the reference held by this function

    // unmounting root filesystem
    auto mountpoint = dentry_unmount(mounted_root.get());
    if (!mountpoint)
    {
        mWarn << "failed to unmount filesystem";
        return -EIO;
    }

    MOS_ASSERT(mounted_root->refcount == mountpoint->refcount && mountpoint->refcount == 1);
    if (mounted_root->superblock->fs->unmount)
        mounted_root->superblock->fs->unmount(mounted_root->superblock->fs, mounted_root.get());
    else
        MOS_ASSERT(dentry_unref_one_norelease(mounted_root.get()));
    MOS_ASSERT_X(mounted_root->refcount == 0, "fs->umount should release the last reference to the mounted root");

    if (mounted_root == root_dentry)
    {
        dInfo2<vfs> << "unmounted root filesystem";
        root_dentry = NULL;
        return 0;
    }

    dentry_unref(mountpoint);
    return 0;
}

PtrResult<FsBaseFile> vfs_openat(int fd, mos::string_view path, OpenFlags flags)
{
    dInfo2<vfs> << "vfs_openat(fd=" << fd << ", path='" << path << "', flags=" << flags << ")";
    auto basedir = path_is_absolute(path) ? root_dentry : dentry_from_fd(fd);
    if (basedir.isErr())
        return basedir.getErr();

    auto file = vfs_do_open(basedir.get(), path, flags);
    return file;
}

long vfs_fstatat(fd_t fd, const char *path, file_stat_t *__restrict statbuf, FStatAtFlags flags)
{
    if (flags & FSTATAT_FILE)
    {
        dInfo2<vfs> << "vfs_fstatat(fd=" << fd << ", path=" << (void *) path << ", stat=" << (void *) statbuf << ", flags=" << flags << ")";
        IO *io = process_get_fd(current_process, fd);
        if (!(IO::IsValid(io) && (io->io_type == IO_FILE || io->io_type == IO_DIR)))
            return -EBADF; // io is closed, or is not a file or directory

        FsBaseFile *file = static_cast<FsBaseFile *>(io);
        MOS_ASSERT(file);
        if (statbuf)
            vfs_copy_stat(statbuf, file->dentry->inode);

        return 0;
    }

    dInfo2<vfs> << "vfs_fstatat(fd=" << fd << ", path='" << path << "', stat=" << (void *) statbuf << ", flags=" << flags << ")";
    auto basedir = path_is_absolute(path) ? root_dentry : dentry_from_fd(fd);
    if (basedir.isErr())
        return basedir.getErr();

    LastSegmentResolveFlags resolve_flags = RESOLVE_EXPECT_ANY_TYPE | RESOLVE_EXPECT_EXIST;
    if (flags & FSTATAT_NOFOLLOW)
        resolve_flags |= RESOLVE_SYMLINK_NOFOLLOW;

    auto dentry = dentry_resolve(basedir.get(), root_dentry, path, resolve_flags);
    if (dentry.isErr())
        return dentry.getErr();

    if (statbuf)
        vfs_copy_stat(statbuf, dentry->inode);
    dentry_unref(dentry.get());
    return 0;
}

size_t vfs_readlinkat(fd_t dirfd, const char *path, char *buf, size_t size)
{
    auto base = path_is_absolute(path) ? root_dentry : dentry_from_fd(dirfd);
    if (base.isErr())
        return base.getErr();

    auto dentry = dentry_resolve(base.get(), root_dentry, path, RESOLVE_SYMLINK_NOFOLLOW | RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_FILE);
    if (dentry.isErr())
        return dentry.getErr();

    if (dentry->inode->type != FILE_TYPE_SYMLINK)
    {
        dentry_unref(dentry.get());
        return -EINVAL;
    }

    const size_t len = dentry->inode->ops->readlink(dentry.get(), buf, size);

    dentry_unref(dentry.get());

    if (len >= size) // buffer too small
        return -ENAMETOOLONG;

    return len;
}

long vfs_symlink(const char *path, const char *target)
{
    dInfo2<vfs> << "vfs_symlink(path='" << path << "', target='" << target << "')";
    auto base = path_is_absolute(path) ? root_dentry : dentry_from_fd(AT_FDCWD);
    if (base.isErr())
        return base.getErr();

    auto dentry = dentry_resolve(base.get(), root_dentry, path, RESOLVE_EXPECT_NONEXIST);
    if (dentry.isErr())
        return dentry.getErr();

    dentry_t *parent_dir = dentry_parent(*dentry);
    const bool created = parent_dir->inode->ops->symlink(parent_dir->inode, dentry.get(), target);

    if (!created)
        mos_warn("failed to create symlink '%s'", path);

    dentry_unref(dentry.get());
    return created ? 0 : -EIO;
}

PtrResult<void> vfs_mkdir(const char *path)
{
    dInfo2<vfs> << "vfs_mkdir('" << path << "')";
    auto base = path_is_absolute(path) ? root_dentry : dentry_from_fd(AT_FDCWD);
    if (base.isErr())
        return base.getErr();

    auto dentry = dentry_resolve(base.get(), root_dentry, path, RESOLVE_EXPECT_NONEXIST);
    if (dentry.isErr())
        return dentry.getErr();

    dentry_t *parent_dir = dentry_parent(*dentry);
    if (parent_dir->inode == NULL || parent_dir->inode->ops == NULL || parent_dir->inode->ops->mkdir == NULL)
    {
        // dentry does not have a mkdir operation
        dentry_unref(dentry.get());
        return -ENOTSUP;
    }

    // TODO: use umask or something else
    const bool created = parent_dir->inode->ops->mkdir(parent_dir->inode, dentry.get(), parent_dir->inode->perm);

    if (!created)
        mos_warn("failed to create directory '%s'", path);

    dentry_unref(dentry.get());
    return created ? 0 : -EIO;
}

PtrResult<void> vfs_rmdir(const char *path)
{
    dInfo2<vfs> << "vfs_rmdir('" << path << "')";
    auto base = path_is_absolute(path) ? root_dentry : dentry_from_fd(AT_FDCWD);
    if (base.isErr())
        return base.getErr();

    auto dentry = dentry_resolve(base.get(), root_dentry, path, RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_DIR);
    if (dentry.isErr())
        return dentry.getErr();

    dentry_t *parent_dir = dentry_parent(*dentry);
    if (parent_dir->inode == NULL || parent_dir->inode->ops == NULL || parent_dir->inode->ops->rmdir == NULL)
    {
        dentry_unref(dentry.get());
        return -ENOTSUP;
    }

    const bool removed = parent_dir->inode->ops->rmdir(parent_dir->inode, dentry.get());

    if (!removed)
        mos_warn("failed to remove directory '%s'", path);

    dentry_unref(dentry.get());
    return removed ? 0 : -EIO;
}

size_t vfs_list_dir(IO *io, void *user_buf, size_t user_size)
{
    dInfo2<vfs> << "vfs_list_dir(io=" << (void *) io << ", buf=" << (void *) user_buf << ", size=" << user_size << ")";
    FsBaseFile *file = static_cast<FsBaseFile *>(io);
    if (unlikely(file->dentry->inode->type != FILE_TYPE_DIRECTORY))
    {
        mos_warn("not a directory");
        return 0;
    }

    if (file->private_data == NULL)
    {
        vfs_listdir_state_t *const state = mos::create<vfs_listdir_state_t>();
        file->private_data = state;
        linked_list_init(&state->entries);
        state->n_count = state->read_offset = 0;
        vfs_populate_listdir_buf(file->dentry, state);
    }

    vfs_listdir_state_t *const state = (vfs_listdir_state_t *) file->private_data;

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

        const size_t entry_size = sizeof(ino_t) + sizeof(off_t) + sizeof(short) + sizeof(char) + entry->name.size() + 1; // +1 for the null terminator
        if (bytes_copied + entry_size > user_size)
            break;

        struct dirent *dirent = (struct dirent *) (((char *) user_buf) + bytes_copied);
        dirent->d_ino = entry->ino;
        dirent->d_type = entry->type;
        dirent->d_reclen = entry_size;
        dirent->d_off = entry_size - 1;
        memcpy(dirent->d_name, entry->name.data(), entry->name.size());
        dirent->d_name[entry->name.size()] = '\0';
        bytes_copied += entry_size;
        state->read_offset++;
    }

    return bytes_copied;
}

long vfs_chdirat(fd_t dirfd, const char *path)
{
    dInfo2<vfs> << "vfs_chdirat('" << dirfd << ", " << path << "')";
    auto base = path_is_absolute(path) ? root_dentry : dentry_from_fd(dirfd);
    if (base.isErr())
        return base.getErr();

    auto dentry = dentry_resolve(base.get(), root_dentry, path, RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_DIR);
    if (dentry.isErr())
        return dentry.getErr();

    auto old_cwd = dentry_from_fd(AT_FDCWD);
    if (old_cwd)
        dentry_unref(old_cwd.get());

    current_process->working_directory = dentry.get();
    return 0;
}

ssize_t vfs_getcwd(char *buf, size_t size)
{
    dInfo2<vfs> << "vfs_getcwd(buf=" << (void *) buf << ", size=" << size << ")";
    auto cwd = dentry_from_fd(AT_FDCWD);
    if (cwd.isErr())
        return cwd.getErr();

    const auto path = dentry_path(cwd.get(), root_dentry);
    if (!path)
        return -ENOMEM;

    const size_t n = path->copy(buf, size);

    if (n != path->size())
        return -ERANGE; // buffer too small

    if (n >= size)
        return -ERANGE; // buffer too small

    buf[n] = '\0'; // null-terminate the string
    return n;
}

long vfs_fchmodat(fd_t fd, const char *path, int perm, int flags)
{
    dInfo2<vfs> << "vfs_fchmodat(fd=" << fd << ", path='" << path << "', perm=" << perm << ", flags=" << flags << ")";
    auto base = path_is_absolute(path) ? root_dentry : dentry_from_fd(fd);
    if (base.isErr())
        return base.getErr();

    auto dentry = dentry_resolve(base.get(), root_dentry, path, RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_ANY_TYPE);
    if (dentry.isErr())
        return dentry.getErr();

    // TODO: check if the underlying filesystem supports chmod, and is not read-only
    dentry->inode->perm = perm;
    dentry_unref(dentry.get());
    return 0;
}

long vfs_unlinkat(fd_t dirfd, const char *path)
{
    dInfo2<vfs> << "vfs_unlinkat(dirfd=" << dirfd << ", path='" << path << "')";
    auto base = path_is_absolute(path) ? root_dentry : dentry_from_fd(dirfd);
    if (base.isErr())
        return base.getErr();

    auto dentry = dentry_resolve(base.get(), root_dentry, path, RESOLVE_EXPECT_EXIST | RESOLVE_EXPECT_FILE | RESOLVE_SYMLINK_NOFOLLOW);
    if (dentry.isErr())
        return dentry.getErr();

    dentry_t *parent_dir = dentry_parent(*dentry);
    if (parent_dir->inode == NULL || parent_dir->inode->ops == NULL || parent_dir->inode->ops->unlink == NULL)
    {
        dentry_unref(dentry.get());
        return -ENOTSUP;
    }

    if (!inode_unlink(parent_dir->inode, dentry.get()))
    {
        dentry_unref(dentry.get());
        return -EIO;
    }

    dentry_unref(dentry.get()); // it won't release dentry because dentry->inode is still valid
    dentry_detach(dentry.get());
    dentry_try_release(dentry.get());
    return 0;
}

long vfs_fsync(IO *io, bool sync_metadata, off_t start, off_t end)
{
    dInfo2<vfs> << "vfs_fsync(io=" << (void *) io << ", sync_metadata=" << sync_metadata << ", start=" << start << ", end=" << end << ")";
    FsBaseFile *file = static_cast<FsBaseFile *>(io);

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
        sysfs_printf(f, "%s\n", fs->name.c_str());
    }

    return true;
}

static bool vfs_sysfs_mountpoints(sysfs_file_t *f)
{
    list_foreach(mount_t, mp, vfs_mountpoint_list)
    {
        const auto str = dentry_path(mp->mountpoint, root_dentry);
        if (str)
            sysfs_printf(f, "%-20s %-10s\n", str->c_str(), mp->fs->name.c_str());
        else
            sysfs_printf(f, "%-20s %-10s\n", "<error>", mp->fs->name.c_str());
    }

    return true;
}

static void vfs_sysfs_dentry_stats_stat_receiver(int depth, const dentry_t *dentry, bool mountroot, void *data)
{
    sysfs_file_t *file = (sysfs_file_t *) data;
    sysfs_printf(file, "%*s%s: refcount=%zu%s\n",                                             //
                 depth * 4,                                                                   //
                 "",                                                                          //
                 dentry_name(dentry).c_str(),                                                 //
                 dentry->refcount.load(),                                                     //
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
