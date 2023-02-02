// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/ring_buffer.h"
#include "mos/mm/ipcshm/ipcshm.h"
#include "mos/tasks/task_types.h"

typedef struct
{
    // srv.w == cli.r == server_writebuf ([server] is writing, or [client] is reading)
    // srv.r == cli.w == client_writebuf ([client] is writing, or [server] is reading)
    struct _ipc_node
    {
        io_t io;
        struct _ipc_node_buf *writebuf_obj, *readbuf_obj; // points to the write buffer for this node, see below
        void *write_buffer, *read_buffer;
    } server, client;

    struct _ipc_node_buf
    {
        ring_buffer_pos_t pos;
        spinlock_t lock;
        bool closed; // set to true when the other node closes this buffer
    } server_nodebuf, client_nodebuf;
} ipc_t;

void ipc_init(void);

/*
 * @brief Create a new IPC server
 * @param name The name of the server
 * @param max_pending_connections The maximum number of pending connections to allow
 * @return A new io_t object that represents the server, or NULL on failure
 *
 * @note The io_t returned by this function is only to accept new connections or close the server, reading or writing to it will fail.
 */
io_t *ipc_create(const char *name, size_t max_pending_connections);

/*
 * @brief Accept a new connection on an IPC server
 * @param server The server to accept a connection on
 * @return A new io_t object that represents the connection to the client, or NULL on failure
 */
io_t *ipc_accept(io_t *server);

/*
 * @brief Connect to an IPC servers
 * @param name The name of the server to connect to
 * @param flags Flags to control the connection
 * @param buffer_size The size of a shared-memory buffer to use for the connection
 * @return A new io_t object that represents the connection, or NULL on failure
 */
io_t *ipc_connect(const char *name, size_t buffer_size);
