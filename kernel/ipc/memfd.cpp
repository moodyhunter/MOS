// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ipc/memfd.hpp"

#include "mos/filesystem/dentry.hpp"
#include "mos/filesystem/inode.hpp"
#include "mos/filesystem/vfs.hpp"
#include "mos/filesystem/vfs_types.hpp"
#include "mos/filesystem/vfs_utils.hpp"
#include "mos/io/io.hpp"
#include "mos/misc/setup.hpp"
#include "mos/syslog/printk.hpp"
#include "mos/types.hpp"

#include <mos/allocator.hpp>
#include <mos/filesystem/fs_types.h>
#include <mos_stdlib.hpp>
#include <stdio.h>

struct memfd_t : mos::NamedType<"memfd">
{
    int unused;
};

extern filesystem_t fs_tmpfs;

static dentry_t *memfd_root_dentry = NULL;

static void memfd_file_release(FsBaseFile *file)
{
    dentry_detach(file->dentry);
}

static const file_ops_t memfd_file_ops = {
    .read = vfs_generic_read,
    .write = vfs_generic_write,
    .release = memfd_file_release,
};

PtrResult<IO> memfd_create(const char *name)
{
    memfd_t *memfd = mos::create<memfd_t>();
    if (!memfd)
    {
        mEmerg << "Failed to allocate memfd";
        return -ENOMEM;
    }

    dentry_t *dentry = dentry_get_from_parent(memfd_root_dentry->superblock, memfd_root_dentry, name);

    if (!memfd_root_dentry->inode->ops->newfile(memfd_root_dentry->inode, dentry, FILE_TYPE_REGULAR, (PERM_READ | PERM_WRITE) & PERM_OWNER))
    {
        mEmerg << "Failed to create file for memfd";
        delete memfd;
        delete dentry;
        return -ENOMEM;
    }

    auto file = vfs_do_open_dentry(dentry, true, true, true, false, false);
    if (file.isErr())
    {
        mEmerg << "Failed to open file for memfd";
        delete memfd;
        delete dentry;
        return file.getErr();
    }

    dentry_ref(dentry), dentry_ref(memfd_root_dentry);
    file->private_data = memfd;
    file->dentry->inode->file_ops = &memfd_file_ops;
    inode_unlink(memfd_root_dentry->inode, file->dentry);
    return file;
}

static void memfd_init()
{
    const auto result = fs_tmpfs.mount(&fs_tmpfs, "none", NULL);
    if (result.isErr())
    {
        mEmerg << "Failed to mount tmpfs for memfd";
        return;
    }

    memfd_root_dentry = result.get();
    if (!memfd_root_dentry)
    {
        mEmerg << "Failed to mount memfd filesystem";
        return;
    }

    memfd_root_dentry->is_mountpoint = true;
    dentry_ref(memfd_root_dentry);
    dentry_ref(memfd_root_dentry);
}

MOS_INIT(VFS, memfd_init);
