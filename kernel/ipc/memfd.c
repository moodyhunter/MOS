// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ipc/memfd.h"

#include "mos/filesystem/dentry.h"
#include "mos/filesystem/vfs.h"
#include "mos/filesystem/vfs_types.h"
#include "mos/filesystem/vfs_utils.h"
#include "mos/io/io.h"
#include "mos/misc/setup.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/syslog/printk.h"

#include <mos/filesystem/fs_types.h>
#include <mos_stdlib.h>
#include <stdio.h>

typedef struct
{
    int unused;
} memfd_t;

extern filesystem_t fs_tmpfs;

static slab_t *memfd_slab = NULL;
SLAB_AUTOINIT("memfd", memfd_slab, memfd_t);

static dentry_t *memfd_root_dentry = NULL;

static void memfd_file_release(file_t *file)
{
    dentry_detach(file->dentry);
}

static const file_ops_t memfd_file_ops = {
    .read = vfs_generic_read,
    .write = vfs_generic_write,
    .release = memfd_file_release,
};

io_t *memfd_create(const char *name)
{
    memfd_t *memfd = kmalloc(memfd_slab);
    if (!memfd)
    {
        pr_emerg("Failed to allocate memfd");
        return ERR_PTR(-ENOMEM);
    }

    dentry_t *dentry = dentry_create(memfd_root_dentry->superblock, memfd_root_dentry, name);

    if (!memfd_root_dentry->inode->ops->newfile(memfd_root_dentry->inode, dentry, FILE_TYPE_REGULAR, (PERM_READ | PERM_WRITE) & PERM_OWNER))
    {
        pr_emerg("Failed to create file for memfd");
        kfree(memfd);
        kfree(dentry);
        return ERR_PTR(-ENOMEM);
    }

    file_t *file = vfs_do_open_dentry(dentry, true, true, true, false, false);
    if (IS_ERR(file))
    {
        pr_emerg("Failed to open file for memfd");
        kfree(memfd);
        kfree(dentry);
        return ERR(file);
    }

    dentry_ref(dentry), dentry_ref(memfd_root_dentry);
    file->private_data = memfd;
    file->dentry->inode->file_ops = &memfd_file_ops;
    memfd_root_dentry->inode->ops->unlink(memfd_root_dentry->inode, file->dentry);

    return &file->io;
}

void memfd_init()
{
    memfd_root_dentry = fs_tmpfs.mount(&fs_tmpfs, "none", NULL);
    if (!memfd_root_dentry)
    {
        pr_emerg("Failed to mount memfd filesystem");
        return;
    }

    memfd_root_dentry->is_mountpoint = true;
    dentry_ref(memfd_root_dentry);
    dentry_ref(memfd_root_dentry);
}

MOS_INIT(VFS, memfd_init);
