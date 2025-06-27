// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.hpp"
#include "mos/types.hpp"

#include <stddef.h>

struct IpcDescriptor;
struct IPCServer;

extern const file_ops_t ipc_sysfs_file_ops;

PtrResult<IPCServer> ipc_server_create(mos::string_view name, size_t max_pending_connections);

PtrResult<IPCServer> ipc_get_server(mos::string_view name);

PtrResult<IpcDescriptor> ipc_server_accept(IPCServer *server);

void ipc_server_close(IPCServer *server);

PtrResult<IpcDescriptor> ipc_connect_to_server(mos::string name, size_t buffer_size);

size_t ipc_client_read(IpcDescriptor *ipc, void *buffer, size_t size);
size_t ipc_client_write(IpcDescriptor *ipc, const void *buffer, size_t size);
size_t ipc_server_read(IpcDescriptor *ipc, void *buffer, size_t size);
size_t ipc_server_write(IpcDescriptor *ipc, const void *buffer, size_t size);

void ipc_client_close_channel(IpcDescriptor *ipc);
void ipc_server_close_channel(IpcDescriptor *ipc);
