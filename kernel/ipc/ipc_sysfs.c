// SPDX-License-Identifier: GPL-3.0-or-later

// ! /sys/ipc/<server_name> operations to connect to an IPC server

#include "mos/filesystem/dentry.h"
#include "mos/filesystem/vfs_types.h"
#include "mos/ipc/ipc.h"
#include "mos/ipc/ipc_io.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/platform/platform.h"
#include "mos/tasks/process.h"

#include <mos_stdlib.h>
#include <mos_string.h>

typedef struct
{
    bool server_control_file;
    ipc_server_t *server;
    ipc_t *client_ipc;
} ipc_vfs_private_t;

slab_t *ipc_vfs_private_slab = NULL;
SLAB_AUTOINIT("ipc_vfs_private", ipc_vfs_private_slab, ipc_vfs_private_t);

static bool vfs_open_ipc(inode_t *ino, file_t *file, bool created)
{
    MOS_UNUSED(ino);

    ipc_t *ipc = NULL;
    ipc_server_t *server = NULL;

    if (created)
    {
        server = ipc_get_server(file->dentry->name);
        MOS_ASSERT(server);
    }
    else
    {
        ipc = ipc_connect_to_server(file->dentry->name, MOS_PAGE_SIZE);
        if (IS_ERR(ipc))
            return false;
    }

    ipc_vfs_private_t *private = kmalloc(ipc_vfs_private_slab);
    private->server_control_file = created;
    private->client_ipc = ipc;
    private->server = server;

    file->private_data = private;
    return true;
}

static ssize_t vfs_ipc_file_read(const file_t *file, void *buf, size_t size, off_t offset)
{
    MOS_UNUSED(offset);

    ipc_vfs_private_t *private = file->private_data;
    if (private->server_control_file)
    {
        // reading from the server's control file accepts and returns a new connection, as a fd
        if (size < sizeof(fd_t))
            return -EINVAL;

        ipc_t *ipc = ipc_server_accept(private->server);
        if (IS_ERR(ipc))
            return PTR_ERR(ipc);

        ipc_conn_io_t *connio = ipc_conn_io_create(ipc, true);
        if (IS_ERR(connio))
            return PTR_ERR(connio);

        const fd_t fd = process_attach_ref_fd(current_process, &connio->io, FD_FLAGS_NONE);
        if (IS_ERR_VALUE(fd))
            return fd;

        // return the fd to the client
        memcpy(buf, &fd, sizeof(fd_t));
        return sizeof(fd_t);
    }
    else
    {
        return ipc_client_read(private->client_ipc, buf, size);
    }
}

static ssize_t vfs_ipc_file_write(const file_t *file, const void *buf, size_t size, off_t offset)
{
    MOS_UNUSED(offset);
    ipc_vfs_private_t *private = file->private_data;
    if (private->server_control_file)
        return -EBADF; // writing to the server's control file is not supported

    return ipc_client_write(private->client_ipc, buf, size);
}

static void vfs_ipc_file_release(file_t *file)
{
    ipc_vfs_private_t *private = file->private_data;
    if (private->server_control_file)
    {
        ipc_server_close(private->server);
        dentry_detach_inode(file->dentry);
    }
    else
        ipc_client_close_channel(private->client_ipc);
    kfree(private);
}

const file_ops_t ipc_sysfs_file_ops = {
    .open = vfs_open_ipc,
    .read = vfs_ipc_file_read,
    .write = vfs_ipc_file_write,
    .release = vfs_ipc_file_release,
};
