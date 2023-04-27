// SPDX-License-Identifier: GPL-3.0-or-later
// An IPC pseudo-filesystem that helps userspace to debug IPC

#include <mos/filesystem/dentry.h>
#include <mos/filesystem/fs_types.h>
#include <mos/filesystem/ipcfs/ipcfs.h>
#include <mos/filesystem/vfs_types.h>
#include <mos/mm/ipcshm/ipcshm.h>
#include <mos/mm/kmalloc.h>
#include <string.h>

filesystem_t fs_ipcfs;

static const file_perm_t ipcfs_default_perm = {
    .group = { .read = true, .write = true, .execute = true },
    .others = { .read = true, .write = true, .execute = true },
    .owner = { .read = true, .write = true, .execute = true },
};

static dentry_t *ipcfs_root_dir;

static inode_t *ipcfs_create_inode(superblock_t *sb, file_type_t type, file_perm_t perm)
{
    static size_t ipcfs_inode_count;

    inode_t *inode = kzalloc(sizeof(inode_t));
    inode->type = type;
    inode->perm = perm;
    inode->superblock = sb;
    inode->ino = ipcfs_inode_count++;

    switch (type)
    {
        case FILE_TYPE_REGULAR:
        case FILE_TYPE_DIRECTORY: break;
        case FILE_TYPE_SYMLINK:
        case FILE_TYPE_CHAR_DEVICE:
        case FILE_TYPE_BLOCK_DEVICE:
        case FILE_TYPE_NAMED_PIPE:
        case FILE_TYPE_SOCKET:
        case FILE_TYPE_UNKNOWN:
        {
            mos_warn("ipcfs: unsupported file type %d", type);
            break;
        }
    }

    return inode;
}

dentry_t *ipcfs_mount(filesystem_t *fs, const char *dev, const char *options)
{
    MOS_ASSERT(fs == &fs_ipcfs);

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

    return ipcfs_root_dir;
}

filesystem_t fs_ipcfs = {
    LIST_NODE_INIT(fs_ipcfs),
    .name = "ipcfs",
    .superblocks = LIST_HEAD_INIT(fs_ipcfs.superblocks),
    .mount = ipcfs_mount,
};

void ipcfs_init(void)
{
    superblock_t *sb = kzalloc(sizeof(superblock_t));

    ipcfs_root_dir = dentry_create(NULL, NULL);
    ipcfs_root_dir->inode = ipcfs_create_inode(sb, FILE_TYPE_DIRECTORY, ipcfs_default_perm);
    ipcfs_root_dir->inode->type = FILE_TYPE_DIRECTORY;
    ipcfs_root_dir->superblock = sb;
}

void ipcfs_register_server(ipcshm_server_t *server)
{
    dentry_create(ipcfs_root_dir, server->name)->inode = ipcfs_create_inode(ipcfs_root_dir->superblock, FILE_TYPE_REGULAR, ipcfs_default_perm);
}

void ipcfs_unregister_server(ipcshm_server_t *server)
{
    dentry_t *dentry = dentry_get_child(ipcfs_root_dir, server->name);
    pr_warn("ipcfs: unregistering server '%s' dentry %p", server->name, (void *) dentry);
}
