// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/vfs_types.hpp"
#include "mos/filesystem/vfs_utils.hpp"
#include "mos/mm/mm.hpp"
#include "mos/mm/physical/pmm.hpp"

#include <algorithm>
#include <mos/allocator.hpp>
#include <mos/filesystem/dentry.hpp>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/misc/setup.hpp>
#include <mos/mos_global.h>
#include <mos/syslog/printk.hpp>
#include <mos/types.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

struct tmpfs_inode_t : mos::NamedType<"TmpFS.Inode">
{
    inode_t real_inode;

    union
    {
        char *symlink_target;
        dev_t dev;
    };
};

struct tmpfs_sb_t : mos::NamedType<"TmpFS.Superblock">
{
    superblock_t sb;
    atomic_t ino;
};

#define TMPFS_INODE(inode) container_of(inode, tmpfs_inode_t, real_inode)
#define TMPFS_SB(var)      container_of(var, tmpfs_sb_t, sb)

static const file_ops_t tmpfs_noop_file_ops = { 0 };

extern const inode_ops_t tmpfs_inode_dir_ops;
extern const inode_ops_t tmpfs_inode_symlink_ops;
extern const inode_cache_ops_t tmpfs_inode_cache_ops;
extern const file_ops_t tmpfs_file_ops;
extern const superblock_ops_t tmpfs_sb_op;

static PtrResult<dentry_t> tmpfs_fsop_mount(filesystem_t *fs, const char *dev, const char *options); // forward declaration
FILESYSTEM_DEFINE(fs_tmpfs, "tmpfs", tmpfs_fsop_mount, NULL);
FILESYSTEM_AUTOREGISTER(fs_tmpfs);

inode_t *tmpfs_create_inode(tmpfs_sb_t *sb, file_type_t type, file_perm_t perm)
{
    tmpfs_inode_t *inode = mos::create<tmpfs_inode_t>();

    inode_init(&inode->real_inode, &sb->sb, ++sb->ino, type);
    inode->real_inode.perm = perm;
    inode->real_inode.cache.ops = &tmpfs_inode_cache_ops;

    switch (type)
    {
        case FILE_TYPE_DIRECTORY:
            // directories
            pr_dinfo2(tmpfs, "tmpfs: creating a directory inode");
            inode->real_inode.ops = &tmpfs_inode_dir_ops;
            inode->real_inode.file_ops = &tmpfs_noop_file_ops;
            break;

        case FILE_TYPE_SYMLINK:
            // symbolic links
            pr_dinfo2(tmpfs, "tmpfs: creating a symlink inode");
            inode->real_inode.ops = &tmpfs_inode_symlink_ops;
            inode->real_inode.file_ops = &tmpfs_noop_file_ops;
            break;

        case FILE_TYPE_REGULAR:
            // regular files, char devices, block devices, named pipes and sockets
            pr_dinfo2(tmpfs, "tmpfs: creating a file inode");
            inode->real_inode.file_ops = &tmpfs_file_ops;
            break;

        case FILE_TYPE_CHAR_DEVICE:
        case FILE_TYPE_BLOCK_DEVICE:
        case FILE_TYPE_NAMED_PIPE:
        case FILE_TYPE_SOCKET:
            mos_warn("TODO: tmpfs: create inode for file type %d", type);
            mos_panic("tmpfs: unsupported file type");
            break;

        case FILE_TYPE_UNKNOWN:
            // unknown file type
            mos_panic("tmpfs: unknown file type");
            break;
    }

    return &inode->real_inode;
}

static const file_perm_t tmpfs_default_mode = PERM_READ | PERM_WRITE | PERM_EXEC; // rwxrwxrwx

static PtrResult<dentry_t> tmpfs_fsop_mount(filesystem_t *fs, const char *dev, const char *options)
{
    MOS_ASSERT(fs == &fs_tmpfs);
    if (strcmp(dev, "none") != 0)
    {
        mos_warn("tmpfs: device not supported");
        return -EINVAL;
    }

    if (options && strlen(options) != 0 && strcmp(options, "defaults") != 0)
    {
        mos_warn("tmpfs: options '%s' not supported", options);
        return -EINVAL;
    }

    tmpfs_sb_t *tmpfs_sb = mos::create<tmpfs_sb_t>();
    tmpfs_sb->sb.fs = fs;
    tmpfs_sb->sb.ops = &tmpfs_sb_op;
    tmpfs_sb->sb.root = dentry_get_from_parent(&tmpfs_sb->sb, NULL, "");
    dentry_attach(tmpfs_sb->sb.root, tmpfs_create_inode(tmpfs_sb, FILE_TYPE_DIRECTORY, tmpfs_default_mode));
    return tmpfs_sb->sb.root;
}

// create a new node in the directory
static bool tmpfs_mknod_impl(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm, dev_t dev)
{
    inode_t *inode = tmpfs_create_inode(TMPFS_SB(dir->superblock), type, perm);
    TMPFS_INODE(inode)->dev = dev;
    dentry_attach(dentry, inode);
    return true;
}

static bool tmpfs_i_create(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm)
{
    return tmpfs_mknod_impl(dir, dentry, type, perm, 0);
}

static bool tmpfs_i_hardlink(dentry_t *old_dentry, inode_t *dir, dentry_t *new_dentry)
{
    MOS_UNUSED(dir);
    MOS_ASSERT_X(old_dentry->inode->type != FILE_TYPE_DIRECTORY, "hard links to directories are insane");
    old_dentry->inode->nlinks++;
    dentry_attach(new_dentry, old_dentry->inode);
    return true;
}

static bool tmpfs_i_symlink(inode_t *dir, dentry_t *dentry, const char *symname)
{
    bool created = tmpfs_mknod_impl(dir, dentry, FILE_TYPE_SYMLINK, tmpfs_default_mode, 0);
    if (created)
    {
        tmpfs_inode_t *inode = TMPFS_INODE(dentry->inode);
        inode->symlink_target = strdup(symname);
    }

    return created;
}

static bool tmpfs_i_unlink(inode_t *dir, dentry_t *dentry)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);
    return true;
}

static bool tmpfs_i_mkdir(inode_t *dir, dentry_t *dentry, file_perm_t perm)
{
    return tmpfs_mknod_impl(dir, dentry, FILE_TYPE_DIRECTORY, perm, 0);
}

static bool tmpfs_i_rmdir(inode_t *dir, dentry_t *subdir_to_remove)
{
    // VFS will ensure that the directory is empty
    MOS_UNUSED(dir);
    MOS_ASSERT(subdir_to_remove->inode->type == FILE_TYPE_DIRECTORY);
    MOS_ASSERT(subdir_to_remove->inode->nlinks == 1); // should be the only link to the directory

    dentry_detach(subdir_to_remove);

    tmpfs_inode_t *inode = TMPFS_INODE(subdir_to_remove->inode);
    delete inode;
    return true;
}

static bool tmpfs_i_mknod(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm, dev_t dev)
{
    return tmpfs_mknod_impl(dir, dentry, type, perm, dev);
}

static bool tmpfs_i_rename(inode_t *old_dir, dentry_t *old_dentry, inode_t *new_dir, dentry_t *new_dentry)
{
    MOS_UNUSED(old_dir);
    MOS_UNUSED(new_dir);
    dentry_attach(new_dentry, old_dentry->inode);
    dentry_detach(old_dentry);
    return true;
}

const inode_ops_t tmpfs_inode_dir_ops = {
    .hardlink = tmpfs_i_hardlink,
    .lookup = NULL, // use kernel's default in-memory lookup
    .mkdir = tmpfs_i_mkdir,
    .mknode = tmpfs_i_mknod,
    .newfile = tmpfs_i_create,
    .rename = tmpfs_i_rename,
    .rmdir = tmpfs_i_rmdir,
    .symlink = tmpfs_i_symlink,
    .unlink = tmpfs_i_unlink,
};

static size_t tmpfs_i_readlink(dentry_t *dentry, char *buffer, size_t buflen)
{
    tmpfs_inode_t *inode = TMPFS_INODE(dentry->inode);
    const size_t bytes_to_copy = std::min(buflen, strlen(inode->symlink_target));
    memcpy(buffer, inode->symlink_target, bytes_to_copy);
    return bytes_to_copy;
}

static PtrResult<phyframe_t> tmpfs_fill_cache(inode_cache_t *cache, uint64_t pgoff)
{
    // if VFS ever calls this function, it means that a file has been
    // written to a new page, but that page has not been allocated yet
    // (i.e. the file has been extended)
    // we don't need to do anything but allocate the page
    MOS_UNUSED(cache);
    MOS_UNUSED(pgoff);

    return pmm_ref_one(mm_get_free_page());
}

static bool tmpfs_sb_drop_inode(inode_t *inode)
{
    tmpfs_inode_t *tmpfs_inode = TMPFS_INODE(inode);
    if (inode->nlinks == 0)
    {
        if (inode->type == FILE_TYPE_DIRECTORY)
            return false;

        if (inode->type == FILE_TYPE_SYMLINK)
            if (tmpfs_inode->symlink_target != NULL)
                kfree(tmpfs_inode->symlink_target);
        delete tmpfs_inode;
    }

    return true;
}

const inode_ops_t tmpfs_inode_symlink_ops = {
    .readlink = tmpfs_i_readlink,
};

const file_ops_t tmpfs_file_ops = {
    .read = vfs_generic_read,
    .write = vfs_generic_write,
};

const inode_cache_ops_t tmpfs_inode_cache_ops = {
    .fill_cache = tmpfs_fill_cache,
    .page_write_begin = simple_page_write_begin,
    .page_write_end = simple_page_write_end,
};

const superblock_ops_t tmpfs_sb_op = {
    .drop_inode = tmpfs_sb_drop_inode,
};
