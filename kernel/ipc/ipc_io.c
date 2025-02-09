// SPDX-License-Identifier: GPL-3.0-or-later
// io_t wrapper for IPC channels

#include "mos/ipc/ipc_io.h"

#include "mos/io/io.h"
#include "mos/ipc/ipc.h"
#include "mos/mm/slab_autoinit.h"

#include <mos_stdlib.h>

typedef struct _ipc_server_io
{
    io_t control_io;
    ipc_server_t *server;
} ipc_server_io_t;

static slab_t *ipc_server_io_slab = NULL;
SLAB_AUTOINIT("ipc_server_io", ipc_server_io_slab, ipc_server_io_t);

static slab_t *ipc_conn_io_slab = NULL;
SLAB_AUTOINIT("ipc_conn_io", ipc_conn_io_slab, ipc_conn_io_t);

static void ipc_control_io_close(io_t *io)
{
    if (io->type != IO_IPC)
        mos_panic("ipc_control_io_close: io->type != IO_IPC"); // handle error cases in io_close

    // we only deannounce the server, we don't free it
    ipc_server_io_t *server_io = container_of(io, ipc_server_io_t, control_io);
    ipc_server_close(server_io->server);
    kfree(server_io);
}

static const io_op_t ipc_control_io_op = {
    .close = ipc_control_io_close,
};

static size_t ipc_client_io_write(io_t *io, const void *buf, size_t size)
{
    ipc_conn_io_t *conn = container_of(io, ipc_conn_io_t, io);
    return ipc_client_write(conn->ipc, buf, size);
}

static size_t ipc_client_io_read(io_t *io, void *buf, size_t size)
{
    ipc_conn_io_t *conn = container_of(io, ipc_conn_io_t, io);
    return ipc_client_read(conn->ipc, buf, size);
}

static void ipc_client_io_close(io_t *io)
{
    if (io->type != IO_IPC)
        mos_panic("ipc_client_io_close: io->type != IO_IPC"); // handle error cases in io_close

    ipc_conn_io_t *conn = container_of(io, ipc_conn_io_t, io);
    ipc_client_close_channel(conn->ipc);
    kfree(conn);
}

static size_t ipc_server_io_write(io_t *io, const void *buf, size_t size)
{
    ipc_conn_io_t *conn = container_of(io, ipc_conn_io_t, io);
    return ipc_server_write(conn->ipc, buf, size);
}

static size_t ipc_server_io_read(io_t *io, void *buf, size_t size)
{
    ipc_conn_io_t *conn = container_of(io, ipc_conn_io_t, io);
    return ipc_server_read(conn->ipc, buf, size);
}

static void ipc_server_io_close(io_t *io)
{
    if (io->type != IO_IPC)
        mos_panic("ipc_server_io_close: io->type != IO_IPC"); // handle error cases in io_close

    ipc_conn_io_t *conn = container_of(io, ipc_conn_io_t, io);
    ipc_server_close_channel(conn->ipc);
    kfree(conn);
}

static const io_op_t ipc_client_io_op = {
    .read = ipc_client_io_read,
    .write = ipc_client_io_write,
    .close = ipc_client_io_close,
};

static const io_op_t ipc_server_io_op = {
    .read = ipc_server_io_read,
    .write = ipc_server_io_write,
    .close = ipc_server_io_close,
};

ipc_conn_io_t *ipc_conn_io_create(ipc_t *ipc, bool is_server_side)
{
    ipc_conn_io_t *io = kmalloc(ipc_conn_io_slab);
    if (IS_ERR(io))
        return ERR(io);
    io->ipc = ipc;
    io_init(&io->io, IO_IPC, IO_READABLE | IO_WRITABLE, is_server_side ? &ipc_server_io_op : &ipc_client_io_op);
    return io;
}

io_t *ipc_create(const char *name, size_t max_pending_connections)
{
    ipc_server_t *server = ipc_server_create(name, max_pending_connections);
    if (IS_ERR(server))
        return ERR(server);

    ipc_server_io_t *io = kmalloc(ipc_server_io_slab);
    if (IS_ERR(io))
        return ERR(io);

    io->server = server;
    io_init(&io->control_io, IO_IPC, IO_NONE, &ipc_control_io_op);
    return &io->control_io;
}

io_t *ipc_accept(io_t *server)
{
    if (server->type != IO_IPC)
        return ERR_PTR(-EBADF); // not an ipc server

    ipc_server_io_t *ipc_server = container_of(server, ipc_server_io_t, control_io);
    ipc_t *ipc = ipc_server_accept(ipc_server->server);
    if (IS_ERR(ipc))
        return ERR(ipc);

    ipc_conn_io_t *io = ipc_conn_io_create(ipc, true);
    if (IS_ERR(io))
        return ERR(io);

    return &io->io;
}

io_t *ipc_connect(const char *name, size_t buffer_size)
{
    ipc_t *ipc = ipc_connect_to_server(name, buffer_size);
    if (IS_ERR(ipc))
        return ERR(ipc);

    ipc_conn_io_t *connio = ipc_conn_io_create(ipc, false);
    if (IS_ERR(connio))
        return ERR(connio);

    return &connio->io;
}
