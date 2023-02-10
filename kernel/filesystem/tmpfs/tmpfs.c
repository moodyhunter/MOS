// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/string.h"
#include "lib/structures/list.h"
#include "mos/filesystem/dentry.h"
#include "mos/filesystem/fs_types.h"
#include "mos/filesystem/vfs.h"
#include "mos/mm/kmalloc.h"
#include "mos/mos_global.h"
#include "mos/printk.h"
#include "mos/setup.h"

typedef struct
{
    superblock_t fs_superblock;
} tmpfs_superblock_t;

typedef struct
{
    inode_t real_inode;

    union
    {
        struct
        {
            list_node_t children;
        } dir;

        struct
        {
            char *target;
        } symlink;

        struct
        {
            size_t size;
            char *data;
        } file;
    };
} tmpfs_inode_t;

static const superblock_ops_t superblock_ops;

static const inode_ops_t tmpfs_inode_dir_ops;
static const inode_ops_t tmpfs_inode_file_ops;
static const inode_ops_t tmpfs_inode_symlink_ops;

static const file_ops_t tmpfs_file_ops;
static const file_ops_t tmpfs_symlink_ops;

static atomic_t tmpfs_inode_count = 0;

should_inline tmpfs_inode_t *INODE(inode_t *inode)
{
    return container_of(inode, tmpfs_inode_t, real_inode);
}

inode_t *tmpfs_create_inode(superblock_t *sb, file_type_t type, file_perm_t perm)
{
    tmpfs_inode_t *inode = kzalloc(sizeof(tmpfs_inode_t));
    inode->real_inode.superblock = sb;

    inode->real_inode.stat.type = type;
    inode->real_inode.stat.perm = perm;

    inode->real_inode.ino = ++tmpfs_inode_count;

    switch (type)
    {
        case FILE_TYPE_DIRECTORY:
            // directories
            mos_debug(tmpfs, "tmpfs: creating a directory inode");
            inode->real_inode.ops = &tmpfs_inode_dir_ops;
            inode->real_inode.file_ops = NULL;
            break;

        case FILE_TYPE_SYMLINK:
            // symbolic links
            mos_debug(tmpfs, "tmpfs: creating a symlink inode");
            inode->real_inode.ops = &tmpfs_inode_symlink_ops;
            inode->real_inode.file_ops = &tmpfs_symlink_ops;
            break;

        case FILE_TYPE_REGULAR:
        case FILE_TYPE_CHAR_DEVICE:
        case FILE_TYPE_BLOCK_DEVICE:
        case FILE_TYPE_NAMED_PIPE:
        case FILE_TYPE_SOCKET:
            // regular files, char devices, block devices, named pipes and sockets
            mos_debug(tmpfs, "tmpfs: creating a file inode");
            inode->real_inode.ops = &tmpfs_inode_file_ops;
            inode->real_inode.file_ops = &tmpfs_file_ops;
            break;

        case FILE_TYPE_UNKNOWN:
            // unknown file type
            mos_panic("tmpfs: unknown file type");
            break;
    }

    return &inode->real_inode;
}

static const file_perm_t tmpfs_default_mode = {
    .owner = { 1, 1, 0 },
    .group = { 1, 1, 0 },
    .others = { 0, 0, 0 },
};

static dentry_t *tmpfs_fsop_mount(filesystem_t *fs, const char *dev, const char *options)
{
    tmpfs_superblock_t *sb = kzalloc(sizeof(tmpfs_superblock_t));
    sb->fs_superblock.ops = &superblock_ops;

    dentry_t *root = dentry_create(NULL, NULL);
    root->inode = tmpfs_create_inode(&sb->fs_superblock, FILE_TYPE_DIRECTORY, tmpfs_default_mode);
    root->inode->stat.type = FILE_TYPE_DIRECTORY;
    root->superblock = &sb->fs_superblock;

    return root;
}

static void tmpfs_release_superblock(superblock_t *sb)
{
    kfree(sb);
}

static bool tmpfs_sb_write_inode(inode_t *inode, bool sync)
{
    // tmpfs doesn't need to write anything to disk
    MOS_UNUSED(inode);
    MOS_UNUSED(sync);
    return true;
}

static bool tmpfs_sb_inode_dirty(inode_t *inode, int flags)
{
    // tmpfs doesn't need to write anything to disk
    MOS_UNUSED(inode);
    MOS_UNUSED(flags);
    return true;
}

static const superblock_ops_t superblock_ops = {
    .write_inode = tmpfs_sb_write_inode,
    .inode_dirty = tmpfs_sb_inode_dirty,
    .release_superblock = tmpfs_release_superblock,
};

// create a new node in the directory
static bool tmpfs_mknod_impl(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm, dev_t dev)
{
    inode_t *inode = tmpfs_create_inode(dir->superblock, type, perm);
    dentry->inode = inode;
    return true;
}

static bool tmpfs_i_lookup(inode_t *dir, dentry_t *dentry)
{
    // Hm, if a dentry doesn't exist in the dcache, it doesn't exist in the filesystem
    MOS_UNUSED(dir);
    MOS_UNUSED(dentry);
    return false;
}

static bool tmpfs_i_create(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm)
{
    return false;
}

static bool tmpfs_i_link(dentry_t *old_dentry, inode_t *dir, dentry_t *new_dentry)
{
    return false;
}

static bool tmpfs_i_symlink(inode_t *dir, dentry_t *dentry, const char *symname)
{
    return false;
}

static bool tmpfs_i_unlink(inode_t *dir, dentry_t *dentry)
{
    return false;
}

static bool tmpfs_i_mkdir(inode_t *dir, dentry_t *dentry, file_perm_t perm)
{
    tmpfs_mknod_impl(dir, dentry, FILE_TYPE_DIRECTORY, perm, 0);
}

static bool tmpfs_i_rmdir(inode_t *dir, dentry_t *dentry)
{
    return false;
}

static bool tmpfs_i_mknod(inode_t *dir, dentry_t *dentry, file_perm_t perm, dev_t dev)
{
    inode_t *inode = tmpfs_create_inode(dir->superblock, FILE_TYPE_REGULAR, perm);
}

static bool tmpfs_i_rename(inode_t *old_dir, dentry_t *old_dentry, inode_t *new_dir, dentry_t *new_dentry)
{
    return false;
}
static const inode_ops_t tmpfs_inode_dir_ops = {
    .lookup = tmpfs_i_lookup,
    .create = tmpfs_i_create,
    .link = tmpfs_i_link,
    .symlink = tmpfs_i_symlink,
    .unlink = tmpfs_i_unlink,
    .mkdir = tmpfs_i_mkdir,
    .rmdir = tmpfs_i_rmdir,
    .mknod = tmpfs_i_mknod,
    .rename = tmpfs_i_rename,
};

static const inode_ops_t tmpfs_inode_file_ops = { 0 };

static bool tmpfs_i_readlink(dentry_t *dentry, char *buffer, size_t buflen)
{
    return false;
}

static const inode_ops_t tmpfs_inode_symlink_ops = {
    .readlink = tmpfs_i_readlink,
};

static ssize_t tmpfs_f_read(file_t *file, void *buffer, size_t buflen)
{
    return 0;
}

static ssize_t tmpfs_f_write(file_t *file, const void *buffer, size_t buflen)
{
    return 0;
}

static const file_ops_t tmpfs_file_ops = {
    .open = NULL, // no special open function required
    .read = tmpfs_f_read,
    .write = tmpfs_f_write,
    .flush = NULL,
    .mmap = NULL, // TODO: use a generic mmap
};

static const filesystem_ops_t fs_tmpfs_ops = {
    .mount = tmpfs_fsop_mount,
};

filesystem_t fs_tmpfs = {
    .list_node = LIST_HEAD_INIT(fs_tmpfs.list_node),
    .name = "tmpfs",
    .ops = &fs_tmpfs_ops,
};
