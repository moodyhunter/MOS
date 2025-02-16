// SPDX-License-Identifier: GPL-3.0-or-later
// io_t wrapper for IPC channels

#include "mos/ipc/ipc_io.hpp"

#include "mos/io/io.hpp"
#include "mos/ipc/ipc.hpp"
#include "mos/misc/panic.hpp"

#include <mos/allocator.hpp>
#include <mos_stdlib.hpp>

struct ipc_server_io_t : mos::NamedType<"IPC.ServerIO">
{
    io_t control_io;
    IPCServer *server;
};

static void ipc_control_io_close(io_t *io)
{
    if (io->type != IO_IPC)
        mos_panic("ipc_control_io_close: io->type != IO_IPC"); // handle error cases in io_close

    // we only deannounce the server, we don't free it
    ipc_server_io_t *server_io = container_of(io, ipc_server_io_t, control_io);
    ipc_server_close(server_io->server);
    delete server_io;
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
    delete conn;
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
    delete conn;
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

PtrResult<ipc_conn_io_t> ipc_conn_io_create(IPCDescriptor *ipc, bool is_server_side)
{
    ipc_conn_io_t *io = mos::create<ipc_conn_io_t>();
    if (io == nullptr)
        return -ENOMEM;

    io->ipc = ipc;
    io_init(&io->io, IO_IPC, IO_READABLE | IO_WRITABLE, is_server_side ? &ipc_server_io_op : &ipc_client_io_op);
    return io;
}

PtrResult<io_t> ipc_create(const char *name, size_t max_pending_connections)
{
    auto server = ipc_server_create(name, max_pending_connections);
    if (server.isErr())
        return server.getErr();

    ipc_server_io_t *io = mos::create<ipc_server_io_t>();
    if (io == nullptr)
    {
        ipc_server_close(server.get());
        return -ENOMEM;
    }

    io->server = server.get();
    io_init(&io->control_io, IO_IPC, IO_NONE, &ipc_control_io_op);
    return &io->control_io;
}

PtrResult<io_t> ipc_accept(io_t *server)
{
    if (server->type != IO_IPC)
        return -EBADF; // not an ipc server

    ipc_server_io_t *ipc_server = container_of(server, ipc_server_io_t, control_io);
    auto ipc = ipc_server_accept(ipc_server->server);
    if (ipc.isErr())
        return ipc.getErr();

    auto io = ipc_conn_io_create(ipc.get(), true);
    if (io.isErr())
        return io.getErr();

    return &io->io;
}

PtrResult<io_t> ipc_connect(const char *name, size_t buffer_size)
{
    auto ipc = ipc_connect_to_server(name, buffer_size);
    if (ipc.isErr())
        return ipc.getErr();

    auto connio = ipc_conn_io_create(ipc.get(), false);
    if (connio.isErr())
        return connio.getErr();

    return &connio->io;
}
