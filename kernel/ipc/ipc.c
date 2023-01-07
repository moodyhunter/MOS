// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ipc/ipc.h"

#include "lib/stdlib.h"
#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "lib/structures/hashmap_common.h"
#include "lib/structures/ring_buffer.h"
#include "lib/sync/refcount.h"
#include "mos/io/io.h"
#include "mos/ipc/ipc_types.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/shm.h"
#include "mos/mutex/mutex.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/task_types.h"
#include "mos/tasks/thread.h"
#include "mos/tasks/wait.h"

#define IPC_CHANNEL_HASHMAP_SIZE 64

static hashmap_t *ipc_servers;

static hash_t ipc_server_hash(const void *key)
{
    return hashmap_hash_string(key);
}

static int ipc_server_compare(const void *a, const void *b)
{
    return strcmp((const char *) a, (const char *) b) == 0;
}

static void ipc_server_close(io_t *io)
{
    pr_info("closing ipc server (io refcount: %zu)", io->refcount);
    ipc_server_t *server = container_of(io, ipc_server_t, io);
    if (server->magic != IPC_SERVER_MAGIC)
    {
        mos_warn("invalid magic");
        return;
    }

    for (size_t i = 0; i < server->max_pending; i++)
    {
        ipc_connection_t *conn = &server->pending[i];
        if (conn->state == IPC_CONNECTION_STATE_PENDING)
            conn->state = IPC_CONNECTION_STATE_CLOSED;
    }

    hashmap_remove(ipc_servers, server->name);
    kfree(server->name);
    kfree(server->pending);
    kfree(server);
}

static const io_op_t ipc_server_ops = {
    .close = ipc_server_close,
};

static ipc_connection_t *ipc_server_get_pending(ipc_server_t *server)
{
    for (size_t i = 0; i < server->max_pending; i++)
    {
        if (server->pending[i].state == IPC_CONNECTION_STATE_PENDING)
            return &server->pending[i];
    }
    return NULL;
}

static bool wc_ipc_name_is_ready(wait_condition_t *cond)
{
    return hashmap_get(ipc_servers, cond->arg) != NULL;
}

static void wc_ipc_name_cleanup(wait_condition_t *cond)
{
    kfree(cond->arg);
}

static bool wc_ipc_has_pending(wait_condition_t *cond)
{
    ipc_server_t *server = cond->arg;
    return ipc_server_get_pending(server) != NULL;
}

static bool wc_ipc_connection_is_connected_or_closed(wait_condition_t *cond)
{
    ipc_connection_t *conn = cond->arg;
    return conn->state == IPC_CONNECTION_STATE_CONNECTED || conn->state == IPC_CONNECTION_STATE_CLOSED;
}

static bool wc_ipc_connection_wait_for_client_data(wait_condition_t *cond)
{
    ipc_connection_t *conn = cond->arg;
    return conn->client_data_size > 0;
}

static bool wc_ipc_connection_wait_for_server_data(wait_condition_t *cond)
{
    ipc_connection_t *conn = cond->arg;
    return conn->server_data_size > 0;
}

static size_t ipc_connection_server_read(io_t *io, void *buf, size_t buf_size)
{
    ipc_connection_t *conn = container_of(io, ipc_connection_t, server_io);
    if (conn->state != IPC_CONNECTION_STATE_CONNECTED)
    {
        pr_warn("connection is not connected");
        return 0;
    }

    mutex_try_acquire_may_reschedule(&conn->mutex);

    if (conn->server_data_size == 0)
    {
        mutex_release(&conn->mutex);
        pr_info2("waiting for client data");
        wait_condition_t *wc = wc_wait_for(conn, wc_ipc_connection_wait_for_server_data, NULL);
        reschedule_for_wait_condition(wc);
        pr_info2("ipc server: client data available");
        mutex_try_acquire_may_reschedule(&conn->mutex);
    }

    const size_t read_size = MIN(conn->server_data_size, buf_size);

    // not enough data to read
    if (read_size == 0)
        return 0;

    // server reads from the back of the ring buffer (where the client writes)
    size_t read = ring_buffer_pos_pop_back((u8 *) conn->server_data_vaddr, &conn->buffer_pos, buf, read_size);
    conn->server_data_size -= read; // for the client

    mutex_release(&conn->mutex);
    return read;
}

static size_t ipc_connection_server_write(io_t *io, const void *buf, size_t size)
{
    ipc_connection_t *conn = container_of(io, ipc_connection_t, server_io);
    if (conn->state != IPC_CONNECTION_STATE_CONNECTED)
    {
        pr_warn("connection is not connected");
        return 0;
    }

    mutex_try_acquire_may_reschedule(&conn->mutex);

    // server writes to the front of the ring buffer (where the client reads)
    size_t written = ring_buffer_pos_push_front((u8 *) conn->server_data_vaddr, &conn->buffer_pos, buf, size);
    conn->client_data_size += written; // for the client

    mutex_release(&conn->mutex);
    return written;
}

static void ipc_connection_server_close(io_t *io)
{
    ipc_connection_t *conn = container_of(io, ipc_connection_t, server_io);
    if (conn->state != IPC_CONNECTION_STATE_CONNECTED)
    {
        pr_warn("connection is not connected");
        return;
    }

    conn->state = IPC_CONNECTION_STATE_CLOSED;
}

static size_t ipc_connection_client_read(io_t *io, void *buf, size_t buf_size)
{
    ipc_connection_t *conn = container_of(io, ipc_connection_t, client_io);
    if (conn->state != IPC_CONNECTION_STATE_CONNECTED)
    {
        pr_warn("connection is not connected");
        return 0;
    }

    mutex_try_acquire_may_reschedule(&conn->mutex);

    if (conn->client_data_size == 0)
    {
        pr_info2("waiting for server data");
        wait_condition_t *wc = wc_wait_for(conn, wc_ipc_connection_wait_for_client_data, NULL);
        mutex_release(&conn->mutex);
        reschedule_for_wait_condition(wc);
        pr_info2("ipc client: server data available");
        mutex_try_acquire_may_reschedule(&conn->mutex);
    }

    const size_t read_size = MIN(conn->client_data_size, buf_size);

    // not enough data
    if (read_size == 0)
        return 0;

    // client reads from the front of the ring buffer (where the server writes)
    size_t read = ring_buffer_pos_pop_front((u8 *) conn->client_data_vaddr, &conn->buffer_pos, buf, read_size);
    conn->client_data_size -= read; // for the server

    mutex_release(&conn->mutex);
    return read;
}

static size_t ipc_connection_client_write(io_t *io, const void *buf, size_t size)
{
    ipc_connection_t *conn = container_of(io, ipc_connection_t, client_io);
    if (conn->state != IPC_CONNECTION_STATE_CONNECTED)
    {
        pr_warn("connection is not connected");
        return 0;
    }

    mutex_try_acquire_may_reschedule(&conn->mutex);

    // client writes to the back of the ring buffer (where the server reads)
    size_t written = ring_buffer_pos_push_back((u8 *) conn->client_data_vaddr, &conn->buffer_pos, buf, size);
    conn->server_data_size += written; // for the server

    mutex_release(&conn->mutex);
    return written;
}

static void ipc_connection_client_close(io_t *io)
{
    ipc_connection_t *conn = container_of(io, ipc_connection_t, client_io);
    if (conn->state != IPC_CONNECTION_STATE_CONNECTED)
    {
        pr_warn("connection is not connected");
        return;
    }

    conn->state = IPC_CONNECTION_STATE_CLOSED;
}

static const io_op_t ipc_connection_server_ops = {
    .read = ipc_connection_server_read,
    .write = ipc_connection_server_write,
    .close = ipc_connection_server_close,
};

static const io_op_t ipc_connection_client_ops = {
    .read = ipc_connection_client_read,
    .write = ipc_connection_client_write,
    .close = ipc_connection_client_close,
};

void ipc_init(void)
{
    pr_info("Initializing IPC subsystem...");
    ipc_servers = kzalloc(sizeof(hashmap_t));
    hashmap_init(ipc_servers, IPC_CHANNEL_HASHMAP_SIZE, ipc_server_hash, ipc_server_compare);
}

io_t *ipc_create(const char *name, size_t max_pending_connections)
{
    ipc_server_t *server = hashmap_get(ipc_servers, name);

    if (unlikely(server))
    {
        pr_warn("IPC channel '%s' already exists", name);
        return NULL;
    }

    pr_info2("creating new channel %s", name);
    server = kzalloc(sizeof(ipc_server_t));
    server->magic = IPC_SERVER_MAGIC;
    server->name = strdup(name);
    server->max_pending = max_pending_connections;
    server->pending = kcalloc(max_pending_connections, sizeof(ipc_connection_t));
    io_init(&server->io, IO_TYPE_NONE, &ipc_server_ops);
    hashmap_put(ipc_servers, server->name, server);

    return &server->io;
}

io_t *ipc_accept(io_t *server)
{
    ipc_server_t *ipc_server = container_of(server, ipc_server_t, io);
    if (ipc_server->magic != IPC_SERVER_MAGIC)
    {
        pr_warn("given io is not an IPC server");
        return NULL;
    }

    ipc_connection_t *conn = NULL;

    // find a pending connection
    for (size_t i = 0; i < ipc_server->max_pending; i++)
    {
        if (ipc_server->pending[i].state == IPC_CONNECTION_STATE_PENDING)
        {
            conn = &ipc_server->pending[i];
            break;
        }
    }

    if (unlikely(!conn))
    {
        // no pending connections, wait for one
        pr_info2("waiting for a pending connection");
        wait_condition_t *cond = wc_wait_for(ipc_server, wc_ipc_has_pending, NULL);
        reschedule_for_wait_condition(cond);
        pr_info2("resuming after pending connection");
        conn = ipc_server_get_pending(ipc_server);
    }

    // accept the connection
    conn->state = IPC_CONNECTION_STATE_CONNECTED;

    // map the shared memory
    vmblock_t shm_block = shm_map_shared_block(conn->shm_block, current_process);
    io_init(&conn->server_io, IO_READABLE | IO_WRITABLE, &ipc_connection_server_ops);
    ring_buffer_pos_init(&conn->buffer_pos, shm_block.npages * MOS_PAGE_SIZE);
    conn->server_data_vaddr = shm_block.vaddr;
    return &conn->server_io;
}

io_t *ipc_connect(process_t *owner, const char *name, ipc_connect_flags flags, size_t buffer_size)
{
    ipc_server_t *server = hashmap_get(ipc_servers, name);

    if (unlikely(!server))
    {
        if (flags & IPC_CONNECT_NONBLOCK)
        {
            pr_warn("IPC channel '%s' does not exist", name);
            return NULL;
        }

        pr_info2("waiting for channel %s to be created", name);

        wait_condition_t *cond = wc_wait_for((void *) strdup(name), wc_ipc_name_is_ready, wc_ipc_name_cleanup);
        reschedule_for_wait_condition(cond);
        pr_info2("resuming after channel %s was created", name);

        server = hashmap_get(ipc_servers, name);
    }

    pr_info2("connecting to channel %s", name);
    // find a pending connection slot
    ipc_connection_t *conn = NULL;
    for (size_t i = 0; i < server->max_pending; i++)
    {
        if (server->pending[i].state == IPC_CONNECTION_STATE_INVALID)
        {
            conn = &server->pending[i];
            break;
        }
    }

    if (unlikely(!conn))
    {
        pr_warn("no pending connection slots available");
        return NULL;
    }

    shm_block_t shm_block = shm_allocate(owner, buffer_size / MOS_PAGE_SIZE, MMAP_PRIVATE, VM_RW); // not user accessible, not child-inheritable

    conn->state = IPC_CONNECTION_STATE_PENDING;
    conn->shm_block = shm_block;
    conn->server = server;
    conn->client_data_vaddr = shm_block.block.vaddr;

    // wait for the server to accept the connection
    wait_condition_t *cond = wc_wait_for(conn, wc_ipc_connection_is_connected_or_closed, NULL);
    reschedule_for_wait_condition(cond);
    if (conn->state == IPC_CONNECTION_STATE_CLOSED)
    {
        pr_warn("connection was closed before it was accepted");
        return NULL;
    }
    pr_info2("resuming after connection was accepted");
    io_init(&conn->client_io, IO_READABLE | IO_WRITABLE, &ipc_connection_client_ops);
    return &conn->client_io;
}
