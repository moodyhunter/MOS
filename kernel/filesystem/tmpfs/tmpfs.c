// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/vfs_types.h"
#include "mos/filesystem/vfs_utils.h"
#include "mos/mm/mm.h"
#include "mos/mm/physical/pmm.h"
#include "mos/mm/slab_autoinit.h"

#include <mos/filesystem/dentry.h>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/vfs.h>
#include <mos/lib/structures/list.h>
#include <mos/misc/setup.h>
#include <mos/mos_global.h>
#include <mos/syslog/printk.h>
#include <mos_stdlib.h>
#include <mos_string.h>

typedef struct
{
    inode_t real_inode;

    union
    {
        char *symlink_target;
        dev_t dev;
    };
} tmpfs_inode_t;

#define TMPFS_INODE(inode) container_of(inode, tmpfs_inode_t, real_inode)

static const inode_ops_t tmpfs_inode_dir_ops;
static const inode_ops_t tmpfs_inode_symlink_ops;
static const inode_cache_ops_t tmpfs_inode_cache_ops;

static const file_ops_t tmpfs_file_ops;

static atomic_t tmpfs_inode_count = 0;

static filesystem_t fs_tmpfs;

static slab_t *tmpfs_inode_cache = NULL;
SLAB_AUTOINIT("tmpfs_inode", tmpfs_inode_cache, tmpfs_inode_t);

should_inline tmpfs_inode_t *INODE(inode_t *inode)
{
    return container_of(inode, tmpfs_inode_t, real_inode);
}

inode_t *tmpfs_create_inode(superblock_t *sb, file_type_t type, file_perm_t perm)
{
    tmpfs_inode_t *inode = kmalloc(tmpfs_inode_cache);

    inode_init(&inode->real_inode, sb, ++tmpfs_inode_count, type);
    inode->real_inode.perm = perm;
    inode->real_inode.cache.ops = &tmpfs_inode_cache_ops;

    switch (type)
    {
        case FILE_TYPE_DIRECTORY:
            // directories
            pr_dinfo2(tmpfs, "tmpfs: creating a directory inode");
            inode->real_inode.ops = &tmpfs_inode_dir_ops;
            inode->real_inode.file_ops = NULL;
            break;

        case FILE_TYPE_SYMLINK:
            // symbolic links
            pr_dinfo2(tmpfs, "tmpfs: creating a symlink inode");
            inode->real_inode.ops = &tmpfs_inode_symlink_ops;
            inode->real_inode.file_ops = NULL;
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

static dentry_t *tmpfs_fsop_mount(filesystem_t *fs, const char *dev, const char *options)
{
    MOS_ASSERT(fs == &fs_tmpfs);
    if (strcmp(dev, "none") != 0)
    {
        mos_warn("tmpfs: device not supported");
        return NULL;
    }

    if (options && strlen(options) != 0 && strcmp(options, "defaults") != 0)
    {
        mos_warn("tmpfs: options '%s' not supported", options);
        return NULL;
    }

    superblock_t *tmpfs_sb = kmalloc(superblock_cache);
    tmpfs_sb->fs = fs;
    tmpfs_sb->root = dentry_create(tmpfs_sb, NULL, NULL);
    tmpfs_sb->root->inode = tmpfs_create_inode(tmpfs_sb, FILE_TYPE_DIRECTORY, tmpfs_default_mode);
    tmpfs_sb->root->inode->type = FILE_TYPE_DIRECTORY;
    return tmpfs_sb->root;
}

// create a new node in the directory
static bool tmpfs_mknod_impl(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm, dev_t dev)
{
    inode_t *inode = tmpfs_create_inode(dir->superblock, type, perm);
    dentry->inode = inode;
    TMPFS_INODE(inode)->dev = dev;
    return true;
}

static bool tmpfs_i_create(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm)
{
    return tmpfs_mknod_impl(dir, dentry, type, perm, 0);
}

static bool tmpfs_i_link(dentry_t *old_dentry, inode_t *dir, dentry_t *new_dentry)
{
    MOS_UNUSED(dir);
    MOS_ASSERT_X(old_dentry->inode->type != FILE_TYPE_DIRECTORY, "hard links to directories are insane");
    old_dentry->inode->nlinks++;
    new_dentry->inode = old_dentry->inode;
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
    dentry->inode->nlinks--;
    if (dentry->inode->nlinks == 0)
    {
        tmpfs_inode_t *inode = TMPFS_INODE(dentry->inode);
        if (inode->real_inode.type == FILE_TYPE_DIRECTORY)
        {
            mos_warn("tmpfs: unlinking a directory");
            return false;
        }

        if (inode->real_inode.type == FILE_TYPE_SYMLINK)
            if (inode->symlink_target != NULL)
                kfree(inode->symlink_target);
        kfree(inode);
    }

    dentry->inode = NULL;
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

    tmpfs_inode_t *inode = TMPFS_INODE(subdir_to_remove->inode);
    kfree(inode);

    subdir_to_remove->inode = NULL;
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
    new_dentry->inode = old_dentry->inode;
    old_dentry->inode = NULL;
    return true;
}

static const inode_ops_t tmpfs_inode_dir_ops = {
    .lookup = NULL, // use kernel's default in-memory lookup
    .newfile = tmpfs_i_create,
    .hardlink = tmpfs_i_link,
    .symlink = tmpfs_i_symlink,
    .unlink = tmpfs_i_unlink,
    .mkdir = tmpfs_i_mkdir,
    .rmdir = tmpfs_i_rmdir,
    .mknode = tmpfs_i_mknod,
    .rename = tmpfs_i_rename,
};

static size_t tmpfs_i_readlink(dentry_t *dentry, char *buffer, size_t buflen)
{
    tmpfs_inode_t *inode = TMPFS_INODE(dentry->inode);
    const size_t bytes_to_copy = MIN(buflen, strlen(inode->symlink_target));
    memcpy(buffer, inode->symlink_target, bytes_to_copy);
    return bytes_to_copy;
}

static phyframe_t *tmpfs_fill_cache(inode_cache_t *cache, off_t pgoff)
{
    // if VFS ever calls this function, it means that a file has been
    // written to a new page, but that page has not been allocated yet
    // (i.e. the file has been extended)
    // we don't need to do anything but allocate the page
    MOS_UNUSED(cache);
    MOS_UNUSED(pgoff);

    return pmm_ref_one(mm_get_free_page());
}

static const inode_ops_t tmpfs_inode_symlink_ops = {
    .readlink = tmpfs_i_readlink,
};

static const file_ops_t tmpfs_file_ops = {
    .read = vfs_generic_read,
    .write = vfs_generic_write,
};

static const inode_cache_ops_t tmpfs_inode_cache_ops = {
    .fill_cache = tmpfs_fill_cache,
    .page_write_begin = simple_page_write_begin,
    .page_write_end = simple_page_write_end,
};

static filesystem_t fs_tmpfs = {
    .list_node = LIST_HEAD_INIT(fs_tmpfs.list_node),
    .name = "tmpfs",
    .mount = tmpfs_fsop_mount,
};

static void register_tmpfs(void)
{
    vfs_register_filesystem(&fs_tmpfs);
}

MOS_INIT(VFS, register_tmpfs);
