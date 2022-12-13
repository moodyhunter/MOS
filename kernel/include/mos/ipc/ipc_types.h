// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/structures/ring_buffer.h"
#include "lib/sync/refcount.h"
#include "mos/io/io.h"
#include "mos/mm/mm_types.h"
#include "mos/tasks/task_types.h"
#include "mos/types.h"

#define IPC_MSG_MAGIC    MOS_FOURCC('I', 'M', 's', 'g')
#define IPC_SERVER_MAGIC MOS_FOURCC('I', 'S', 'r', 'v')
#define IPC_CLIENT_MAGIC MOS_FOURCC('I', 'C', 'l', 't')

typedef enum
{
    IPC_CONNECT_DEFAULT = 0,
    IPC_CONNECT_NONBLOCK = 1 << 0,
} ipc_connect_flags;

typedef enum
{
    IPC_CONNECTION_STATE_INVALID = 0, // invalid connection (not connected)
    IPC_CONNECTION_STATE_PENDING,     // pending server accept
    IPC_CONNECTION_STATE_CONNECTED,   // connected
    IPC_CONNECTION_STATE_CLOSED,
} ipc_connection_state;

typedef enum
{
    IPC_MSG_TYPE_DATA,
    IPC_MSG_TYPE_CONNECT,
    IPC_MSG_TYPE_DISCONNECT,
    IPC_MSG_TYPE_DISCONNECT_ACK,
    IPC_MSG_TYPE_CONNECT_ACK,
} ipc_msg_type;

typedef struct
{
    u32 magic;
    ipc_msg_type type;
    u32 length;
    char data[]; // variable length data
} __packed ipc_msg_t;

typedef struct ipc_connection_t ipc_connection_t;
typedef struct ipc_server_t ipc_server_t;

typedef struct ipc_server_t
{
    u32 magic;
    const char *name;
    // a server socket does not implement any read/write operations
    // (it's essentially a file descriptor that to 'accept' new connections)
    // only the close operation is implemented
    io_t io;

    size_t max_pending;
    ipc_connection_t *pending;
} ipc_server_t;

typedef struct ipc_connection_t
{
    ipc_server_t *server;
    ipc_connection_state state;
    shm_block_t shm_block;
    io_t server_io, client_io;
    uintptr_t server_data_vaddr, client_data_vaddr;
    size_t server_data_size, client_data_size;
    ring_buffer_pos_t buffer_pos;
} ipc_connection_t;
