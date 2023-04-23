// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/filesystem/ipcfs/ipcfs.h>
#include <mos/io/io.h>
#include <mos/ipc/ipc.h>
#include <mos/lib/structures/ring_buffer.h>
#include <mos/mm/ipcshm/ipcshm.h>
#include <mos/mm/kmalloc.h>
#include <mos/mos_global.h>
#include <mos/printk.h>
#include <mos/tasks/schedule.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/wait.h>

#define IPC_SERVER_MAGIC MOS_FOURCC('I', 'P', 'C', 'S')

typedef struct
{
    u32 magic;
    io_t io;
    ipcshm_server_t *shm_server;
} ipc_server_t;

static void ipc_server_io_close(io_t *io)
{
    ipc_server_t *ipc_server = container_of(io, ipc_server_t, io);
    if (ipc_server->magic != IPC_SERVER_MAGIC)
    {
        pr_warn("ipc_server_close: invalid magic");
        return;
    }

    ipcshm_server_t *shm_server = ipc_server->shm_server;
    ipcshm_deannounce(shm_server); // shm_server is freed by ipcshm_deannounce
    kfree(ipc_server);
    // existing connections are not closed (they have their own io_t)
}

static size_t ipc_io_write(io_t *io, const void *buf, size_t size)
{
    struct _ipc_node *ipc_node = container_of(io, struct _ipc_node, io);
    struct _ipc_node_buf *writebuf = ipc_node->writebuf_obj;

    // write data to buffer
    spinlock_acquire(&writebuf->lock);
    size_t written = ring_buffer_pos_push_back(ipc_node->write_buffer, &writebuf->pos, buf, size);
    spinlock_release(&writebuf->lock);
    if (written != size)
        pr_warn("ipc_conn_write: could not write all data to buffer (written %zu, size %zu)", written, size);
    return written;
}

static bool buffer_pos_ready_for_read(wait_condition_t *wc)
{
    return !ring_buffer_pos_is_empty((ring_buffer_pos_t *) wc->arg);
}

static wait_condition_t *wc_wait_for_buffer_ready_read(ring_buffer_pos_t *pos)
{
    return wc_wait_for(pos, buffer_pos_ready_for_read, NULL);
}

static size_t ipc_io_read(io_t *io, void *buf, size_t size)
{
    struct _ipc_node *ipc_node = container_of(io, struct _ipc_node, io);
    struct _ipc_node_buf *readbuf = ipc_node->readbuf_obj;

    // read data from buffer
    // unfortunately this is a blocking read, we have to reschedule
    spinlock_acquire(&readbuf->lock);
    while (ring_buffer_pos_is_empty(&readbuf->pos))
    {
        mos_debug(ipc, "tid %ld buffer empty, rescheduling", current_thread->tid);
        spinlock_release(&readbuf->lock);
        reschedule_for_wait_condition(wc_wait_for_buffer_ready_read(&readbuf->pos));
        spinlock_acquire(&readbuf->lock);
        mos_debug(ipc, "tid %ld rescheduled", current_thread->tid);
    }
    size_t read = ring_buffer_pos_pop_front(ipc_node->read_buffer, &readbuf->pos, buf, size);
    spinlock_release(&readbuf->lock);
    return read;
}

static void ipc_io_close(io_t *io)
{
    // the refcount drops to 0, so these are definitely okay to be freed
    struct _ipc_node *ipc_node = container_of(io, struct _ipc_node, io);
    MOS_UNUSED(ipc_node);
    pr_warn("ipc connection closed, TODO: unmap shared memory and kfree()");
}

static const io_op_t ipc_server_op = {
    .read = NULL,
    .write = NULL,
    .close = ipc_server_io_close,
};

static const io_op_t ipc_connection_op = {
    .read = ipc_io_read,
    .write = ipc_io_write,
    .close = ipc_io_close,
};

void ipc_init(void)
{
    pr_info2("initializing IPC subsystem");
    ipcfs_init();
    ipcshm_init();
}

io_t *ipc_create(const char *name, size_t max_pending)
{
    ipcshm_server_t *server = ipcshm_announce(name, max_pending);
    if (!server)
        return NULL;

    ipc_server_t *ipc_server = kzalloc(sizeof(ipc_server_t));
    ipc_server->shm_server = server;
    ipc_server->magic = IPC_SERVER_MAGIC;
    io_init(&ipc_server->io, IO_IPC, IO_NONE, &ipc_server_op);
    return &ipc_server->io;
}

io_t *ipc_accept(io_t *server)
{
    ipc_server_t *ipc_server = container_of(server, ipc_server_t, io);
    if (ipc_server->magic != IPC_SERVER_MAGIC)
    {
        pr_warn("ipc_accept: invalid magic");
        return NULL;
    }
    ipcshm_server_t *shm_server = ipc_server->shm_server;

    void *write_buf = NULL, *read_buf = NULL, *ipc_ptr = NULL; // ipc will be set to the ipc_t of the new connection (created by ipc_connect)

    bool accepted = ipcshm_accept(shm_server, &read_buf, &write_buf, (void **) &ipc_ptr);
    if (!accepted)
        return NULL;

    ipc_t *ipc = (ipc_t *) ipc_ptr;
    ipc->server.read_buffer = read_buf;
    ipc->server.write_buffer = write_buf;

    io_init(&ipc->server.io, IO_IPC, IO_READABLE | IO_WRITABLE, &ipc_connection_op);
    return &ipc->server.io;
}

io_t *ipc_connect(const char *name, size_t buffer_size)
{
    ipc_t *ipc = kzalloc(sizeof(ipc_t));

    // the server's write buffer is the client's read buffer and vice versa
    ipc->server.writebuf_obj = ipc->client.readbuf_obj = &ipc->server_nodebuf;
    ipc->client.writebuf_obj = ipc->server.readbuf_obj = &ipc->client_nodebuf;

    ring_buffer_pos_init(&ipc->server_nodebuf.pos, buffer_size);
    ring_buffer_pos_init(&ipc->client_nodebuf.pos, buffer_size);

    ipcshm_t *shm = ipcshm_request(name, buffer_size, &ipc->client.read_buffer, &ipc->client.write_buffer, ipc);
    if (!shm)
    {
        kfree(ipc);
        return NULL;
    }

    io_init(&ipc->client.io, IO_IPC, IO_READABLE | IO_WRITABLE, &ipc_connection_op);
    return io_ref(&ipc->client.io);
}
