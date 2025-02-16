// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/filesystem/vfs_types.hpp"
#include "mos/types.hpp"

#include <stddef.h>

struct IPCDescriptor;
struct IPCServer;

extern const file_ops_t ipc_sysfs_file_ops;

void ipc_init(void);

PtrResult<IPCServer> ipc_server_create(mos::string_view name, size_t max_pending_connections);

PtrResult<IPCServer> ipc_get_server(mos::string_view name);

PtrResult<IPCDescriptor> ipc_server_accept(IPCServer *server);

void ipc_server_close(IPCServer *server);

PtrResult<IPCDescriptor> ipc_connect_to_server(mos::string_view name, size_t buffer_size);

size_t ipc_client_read(IPCDescriptor *ipc, void *buffer, size_t size);
size_t ipc_client_write(IPCDescriptor *ipc, const void *buffer, size_t size);
size_t ipc_server_read(IPCDescriptor *ipc, void *buffer, size_t size);
size_t ipc_server_write(IPCDescriptor *ipc, const void *buffer, size_t size);

void ipc_client_close_channel(IPCDescriptor *ipc);
void ipc_server_close_channel(IPCDescriptor *ipc);
