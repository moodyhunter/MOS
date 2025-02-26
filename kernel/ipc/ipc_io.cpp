// SPDX-License-Identifier: GPL-3.0-or-later
// IO wrapper for IPC channels

#include "mos/ipc/ipc_io.hpp"

#include "mos/io/io.hpp"
#include "mos/ipc/ipc.hpp"
#include "mos/misc/panic.hpp"

#include <mos/allocator.hpp>
#include <mos_stdlib.hpp>

struct IPC_ControlIO : IO
{
    IPC_ControlIO() : IO(IO_NONE, IO_IPC) {};
    void on_closed();
};

struct ipc_server_io_t : mos::NamedType<"IPC.ServerIO">
{
    IPC_ControlIO control_io;
    IPCServer *server;
};

void IPC_ControlIO::on_closed()
{
    if (io_type != IO_IPC)
        mos_panic("ipc_control_io_close: io->type != IO_IPC"); // handle error cases in io_close

    // we only deannounce the server, we don't free it
    ipc_server_io_t *server_io = container_of(this, ipc_server_io_t, control_io);
    ipc_server_close(server_io->server);
    delete server_io;
}

struct IpcServerIO : IpcConnectionIO, mos::NamedType<"IPC.ServerIO">
{
    IpcServerIO(IpcDescriptor *desc) : IpcConnectionIO(desc) {};
    virtual ~IpcServerIO() {};

    size_t on_read(void *buf, size_t size)
    {
        return ipc_server_read(descriptor, buf, size);
    }
    size_t on_write(const void *buf, size_t size)
    {
        return ipc_server_write(descriptor, buf, size);
    }
    void on_closed()
    {
        ipc_server_close_channel(descriptor);
    }
};

struct IpcClientIO : IpcConnectionIO, mos::NamedType<"IPC.ClientIO">
{
    IpcClientIO(IpcDescriptor *desc) : IpcConnectionIO(desc) {};
    virtual ~IpcClientIO() {};

    size_t on_read(void *buf, size_t size)
    {
        return ipc_client_read(descriptor, buf, size);
    }
    size_t on_write(const void *buf, size_t size)
    {
        return ipc_client_write(descriptor, buf, size);
    }
    void on_closed()
    {
        ipc_client_close_channel(descriptor);
    }
};

PtrResult<IpcConnectionIO> ipc_conn_io_create(IpcDescriptor *desc, bool isServerSide)
{
    if (isServerSide)
    {
        auto io = mos::create<IpcServerIO>(desc);
        if (io == nullptr)
            return -ENOMEM;
        return io;
    }
    else
    {
        auto io = mos::create<IpcClientIO>(desc);
        if (io == nullptr)
            return -ENOMEM;
        return io;
    }
}

PtrResult<IO> ipc_create(const char *name, size_t max_pending_connections)
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
    return &io->control_io;
}

PtrResult<IO> ipc_accept(IO *server)
{
    if (server->io_type != IO_IPC)
        return -EBADF; // not an ipc server

    ipc_server_io_t *ipc_server = container_of(static_cast<IPC_ControlIO *>(server), ipc_server_io_t, control_io);
    const auto ipc = ipc_server_accept(ipc_server->server);
    if (ipc.isErr())
        return ipc.getErr();

    const auto io = ipc_conn_io_create(ipc.get(), true);
    if (io.isErr())
        return io.getErr();

    return io;
}

PtrResult<IO> ipc_connect(const char *name, size_t buffer_size)
{
    const auto ipc = ipc_connect_to_server(name, buffer_size);
    if (ipc.isErr())
        return ipc.getErr();

    const auto io = ipc_conn_io_create(ipc.get(), false);
    if (io.isErr())
        return io.getErr();

    return io;
}
