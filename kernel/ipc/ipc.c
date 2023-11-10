// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/filesystem/vfs_types.h"
#include "mos/filesystem/vfs_utils.h"
#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"

#include <mos/filesystem/fs_types.h>
#include <mos/io/io.h>
#include <mos/ipc/ipc.h>
#include <mos/lib/structures/list.h>
#include <mos/lib/structures/ring_buffer.h>
#include <mos/mm/ipcshm/ipcshm.h>
#include <mos/mos_global.h>
#include <mos/printk.h>
#include <mos/tasks/schedule.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/wait.h>
#include <mos_stdlib.h>
#include <mos_string.h>

#define IPC_SERVER_MAGIC MOS_FOURCC('I', 'P', 'C', 'S')

static list_head ipc_servers = LIST_HEAD_INIT(ipc_servers);

static slab_t *ipc_slab = NULL;
SLAB_AUTOINIT("ipc", ipc_slab, ipc_t);

typedef struct
{
    as_linked_list;
    u32 magic;
    io_t io;
    ipcshm_server_t *shm_server;
    inode_t *sysfs_ino;
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
    list_remove(ipc_server);
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
        pr_dinfo2(ipc, "tid %pt buffer empty, rescheduling", (void *) current_thread);
        spinlock_release(&readbuf->lock);
        reschedule_for_wait_condition(wc_wait_for_buffer_ready_read(&readbuf->pos));
        spinlock_acquire(&readbuf->lock);
        pr_dinfo2(ipc, "tid %pt rescheduled", (void *) current_thread);
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
    ipcshm_init();
}

io_t *ipc_create(const char *name, size_t max_pending)
{
    ipcshm_server_t *server = ipcshm_announce(name, max_pending);
    if (!server)
        return NULL;

    ipc_server_t *ipc_server = kmalloc(sizeof(ipc_server_t));
    linked_list_init(list_node(ipc_server));
    ipc_server->shm_server = server;
    ipc_server->magic = IPC_SERVER_MAGIC;
    io_init(&ipc_server->io, IO_IPC, IO_NONE, &ipc_server_op);
    list_node_append(&ipc_servers, list_node(ipc_server));
    return &ipc_server->io;
}

io_t *ipc_accept(io_t *server)
{
    if (!server)
        return NULL;

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
    ipc_t *ipc = kmalloc(ipc_slab);

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

// ! sysfs support

static bool ipc_sysfs_servers(sysfs_file_t *f)
{
    sysfs_printf(f, "%-20s\t%s\n", "Server Name", "Max Pending Connections");
    list_foreach(ipc_server_t, ipc, ipc_servers)
    {
        sysfs_printf(f, "%-20s\t%zu\n", ipc->shm_server->name, ipc->shm_server->max_pending);
    }

    return true;
}

static inode_t *ipc_sysfs_create_ino(ipc_server_t *ipc_server)
{
    ipc_server->sysfs_ino = sysfs_create_inode(FILE_TYPE_DIRECTORY, ipc_server);
    ipc_server->sysfs_ino->perm = PERM_OWNER & (PERM_READ | PERM_WRITE);
    return ipc_server->sysfs_ino;
}

static size_t ipc_sysfs_list_ipcs(sysfs_item_t *item, dentry_t *d, dir_iterator_state_t *state, dentry_iterator_op op)
{
    MOS_UNUSED(item);
    MOS_UNUSED(d);

    size_t i = state->i - state->start_nth;
    size_t written = 0;

    list_foreach(ipc_server_t, ipc_server, ipc_servers)
    {
        // skip entries until we reach the nth one
        if (state->i != i++)
            continue;

        if (!ipc_server->sysfs_ino)
        {
            if (!ipc_sysfs_create_ino(ipc_server))
                MOS_UNREACHABLE();
        }

        const size_t w = op(state, ipc_server->sysfs_ino->ino, ipc_server->shm_server->name, strlen(ipc_server->shm_server->name), FILE_TYPE_REGULAR);
        if (w == 0)
            break;
        written += w;
    }

    return written;
}

static bool ipc_sysfs_lookup_ipc(inode_t *parent_dir, dentry_t *dentry)
{
    MOS_UNUSED(parent_dir);

    const char *name = dentry->name;
    ipc_server_t *ipc_server = NULL;
    list_foreach(ipc_server_t, ipc, ipc_servers)
    {
        if (strcmp(ipc->shm_server->name, name) == 0)
        {
            ipc_server = ipc;
            break;
        }
    }

    if (ipc_server->sysfs_ino)
    {
        dentry->inode = ipc_server->sysfs_ino;
        return true;
    }

    dentry->inode = ipc_sysfs_create_ino(ipc_server);
    return true;
}

static sysfs_item_t ipc_sysfs_items[] = {
    SYSFS_RO_ITEM("servers", ipc_sysfs_servers),
    SYSFS_DYN_ITEMS("ipcs", ipc_sysfs_list_ipcs, ipc_sysfs_lookup_ipc),
};

SYSFS_AUTOREGISTER(ipc, ipc_sysfs_items);
