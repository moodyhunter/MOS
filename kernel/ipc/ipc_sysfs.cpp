// SPDX-License-Identifier: GPL-3.0-or-later

// ! /sys/ipc/<server_name> operations to connect to an IPC server

#include "mos/filesystem/dentry.hpp"
#include "mos/filesystem/vfs_types.hpp"
#include "mos/ipc/ipc.hpp"
#include "mos/ipc/ipc_io.hpp"
#include "mos/platform/platform.hpp"
#include "mos/tasks/process.hpp"

#include <mos/allocator.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

struct ipc_vfs_private_t : mos::NamedType<"IPC_VFS_Private">
{
    bool server_control_file;
    IPCServer *server;
    IPCDescriptor *client_ipc;
};

static bool vfs_open_ipc(inode_t *ino, file_t *file, bool created)
{
    MOS_UNUSED(ino);

    IPCDescriptor *ipc = NULL;
    IPCServer *server = NULL;

    if (created)
    {
        const auto result = ipc_get_server(file->dentry->name);
        if (result.isErr())
            return false;
        server = result.get();
        MOS_ASSERT(server);
    }
    else
    {
        const auto result = ipc_connect_to_server(file->dentry->name, MOS_PAGE_SIZE);
        if (result.isErr())
            return false;
        ipc = result.get();
        MOS_ASSERT(ipc);
    }

    ipc_vfs_private_t *priv = mos::create<ipc_vfs_private_t>();
    priv->server_control_file = created;
    priv->client_ipc = ipc;
    priv->server = server;

    file->private_data = priv;
    return true;
}

static ssize_t vfs_ipc_file_read(const file_t *file, void *buf, size_t size, off_t offset)
{
    MOS_UNUSED(offset);

    ipc_vfs_private_t *priv = (ipc_vfs_private_t *) file->private_data;
    if (priv->server_control_file)
    {
        // reading from the server's control file accepts and returns a new connection, as a fd
        if (size < sizeof(fd_t))
            return -EINVAL;

        auto ipc = ipc_server_accept(priv->server);
        if (ipc.isErr())
            return ipc.getErr();

        auto connio = ipc_conn_io_create(ipc.get(), true);
        if (connio.isErr())
            return connio.getErr();

        const fd_t fd = process_attach_ref_fd(current_process, &connio->io, FD_FLAGS_NONE);
        if (IS_ERR_VALUE(fd))
            return fd;

        // return the fd to the client
        memcpy(buf, &fd, sizeof(fd_t));
        return sizeof(fd_t);
    }
    else
    {
        return ipc_client_read(priv->client_ipc, buf, size);
    }
}

static ssize_t vfs_ipc_file_write(const file_t *file, const void *buf, size_t size, off_t offset)
{
    MOS_UNUSED(offset);
    ipc_vfs_private_t *priv = (ipc_vfs_private_t *) file->private_data;
    if (priv->server_control_file)
        return -EBADF; // writing to the server's control file is not supported

    return ipc_client_write(priv->client_ipc, buf, size);
}

static void vfs_ipc_file_release(file_t *file)
{
    ipc_vfs_private_t *priv = (ipc_vfs_private_t *) file->private_data;
    if (priv->server_control_file)
    {
        ipc_server_close(priv->server);
        dentry_detach(file->dentry);
    }
    else
        ipc_client_close_channel(priv->client_ipc);
    delete priv;
}

const file_ops_t ipc_sysfs_file_ops = {
    .open = vfs_open_ipc,
    .read = vfs_ipc_file_read,
    .write = vfs_ipc_file_write,
    .release = vfs_ipc_file_release,
};
