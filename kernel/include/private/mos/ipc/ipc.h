// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stddef.h>

typedef struct _ipc ipc_t;
typedef struct _ipc_server ipc_server_t;

void ipc_init(void);

ipc_server_t *ipc_server_create(const char *name, size_t max_pending_connections);

ipc_server_t *ipc_get_server(const char *name);

ipc_t *ipc_server_accept(ipc_server_t *server);

void ipc_server_close(ipc_server_t *server);

ipc_t *ipc_connect_to_server(const char *name, size_t buffer_size);

size_t ipc_client_read(ipc_t *ipc, void *buffer, size_t size);
size_t ipc_client_write(ipc_t *ipc, const void *buffer, size_t size);
size_t ipc_server_read(ipc_t *ipc, void *buffer, size_t size);
size_t ipc_server_write(ipc_t *ipc, const void *buffer, size_t size);

void ipc_client_close_channel(ipc_t *ipc);
void ipc_server_close_channel(ipc_t *ipc);
