// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/io/io.h"
#include "mos/ipc/ipc.h"
#include "mos/platform/platform.h"

typedef struct
{
    io_t io;
    ipc_t *ipc;
} ipc_conn_io_t;

/**
 * @brief Create a new IPC server
 * @param name The name of the server
 * @param max_pending_connections The maximum number of pending connections to allow
 * @return A new io_t object that represents the server, or an error code on failure
 *
 * @note The io_t returned by this function is only to accept new connections or close the server, reading or writing to it will fail.
 */
io_t *ipc_create(const char *name, size_t max_pending_connections);

/**
 * @brief Accept a new connection on an IPC server
 * @param server The server to accept a connection on
 * @return An io_t for the server side of the connection, or an error code on failure
 */
io_t *ipc_accept(io_t *server);

/**
 * @brief Connect to an IPC servers
 * @param name The name of the server to connect to
 * @param buffer_size The size of a shared-memory buffer to use for the connection
 * @return A new io_t object that represents the connection, or an error code on failure
 */
io_t *ipc_connect(const char *name, size_t buffer_size);

/**
 * @brief Create a new IPC connection io descriptor
 *
 * @param ipc The IPC object to create the connection for
 * @param is_server_side Whether this is the server side of the connection
 * @return ipc_conn_io_t* A new IPC connection io descriptor
 */
ipc_conn_io_t *ipc_conn_io_create(ipc_t *ipc, bool is_server_side);
