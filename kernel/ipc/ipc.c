// SPDX-License-Identifier: GPL-3.0-or-later

#define pr_fmt(fmt) "ipc: " fmt

#include "mos/ipc/ipc.h"

#include "mos/filesystem/sysfs/sysfs.h"
#include "mos/filesystem/sysfs/sysfs_autoinit.h"
#include "mos/filesystem/vfs_types.h"
#include "mos/ipc/pipe.h"
#include "mos/mm/slab.h"
#include "mos/mm/slab_autoinit.h"
#include "mos/platform/platform.h"
#include "mos/printk.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/signal.h"
#include "mos/tasks/wait.h"

#include <mos/filesystem/fs_types.h>
#include <mos/lib/structures/hashmap_common.h>
#include <mos/lib/structures/list.h>
#include <mos/mos_global.h>
#include <mos_stdlib.h>
#include <mos_string.h>

#define IPC_SERVER_MAGIC MOS_FOURCC('I', 'P', 'C', 'S')

typedef struct _ipc
{
    as_linked_list; ///< attached to either pending or established list
    size_t buffer_size_npages;
    const char *server_name;

    waitlist_t client_waitlist; ///< client waits here for the server to accept the connection

    union
    {
        pipe_t *client_write_pipe;
        pipe_t *server_read_pipe;
    };

    union
    {
        pipe_t *server_write_pipe;
        pipe_t *client_read_pipe;
    };
} ipc_t;

typedef struct _ipc_server
{
    as_linked_list;
    const char *name;
    spinlock_t lock;
    inode_t *sysfs_ino; ///< inode for sysfs
    size_t pending_max, pending_n;
    size_t established_n;
    list_head pending;     ///< list of ipc_t
    list_head established; ///< list of ipc_t

    waitlist_t server_waitlist; ///< wake up the server here when a client connects
} ipc_server_t;

static slab_t *ipc_server_slab = NULL;
SLAB_AUTOINIT("ipc_server", ipc_server_slab, ipc_server_t);

static slab_t *ipc_slab = NULL;
SLAB_AUTOINIT("ipc", ipc_slab, ipc_t);

static list_head ipc_servers = LIST_HEAD_INIT(ipc_servers);
static hashmap_t name_waitlist;             // waitlist for an IPC server, key = name, value = waitlist_t *
static spinlock_t ipc_lock = SPINLOCK_INIT; ///< protects ipc_servers and name_waitlist

void ipc_server_close(ipc_server_t *server)
{
    spinlock_acquire(&ipc_lock);
    spinlock_acquire(&server->lock);
    // remove the server from the list
    list_remove(server);

    waitlist_t *waitlist = hashmap_get(&name_waitlist, (ptr_t) server->name);
    if (waitlist)
    {
        // the waitlist should have been closed when the server was created
        // and no one should be waiting on it
        spinlock_acquire(&waitlist->lock);
        MOS_ASSERT(waitlist->closed);
        MOS_ASSERT(list_is_empty(&waitlist->list));
        spinlock_release(&waitlist->lock);
        waitlist_init(waitlist); // reuse the waitlist
    }
    spinlock_release(&ipc_lock);

    // with the server lock held, we reject all pending connections
    list_foreach(ipc_t, ipc, server->pending)
    {
        ipc->buffer_size_npages = 0; // mark the connection as closed
        // wake up the client
        waitlist_close(&ipc->client_waitlist);
        waitlist_wake_all(&ipc->client_waitlist);
    }

    server->pending_max = 0;
    waitlist_close(&server->server_waitlist);            // close the server's waitlist
    int n = waitlist_wake_all(&server->server_waitlist); // wake up the server, if it is waiting

    if (n)
    {
        spinlock_release(&server->lock);
        // let the accept() syscall free the server
    }
    else
    {
        // now we can free the server
        kfree(server->name);
        spinlock_release(&server->lock);
        kfree(server);
    }
}

size_t ipc_client_read(ipc_t *ipc, void *buf, size_t size)
{
    return pipe_read(ipc->client_read_pipe, buf, size);
}

size_t ipc_client_write(ipc_t *ipc, const void *buf, size_t size)
{
    return pipe_write(ipc->client_write_pipe, buf, size);
}

size_t ipc_server_read(ipc_t *ipc, void *buf, size_t size)
{
    return pipe_read(ipc->server_read_pipe, buf, size);
}

size_t ipc_server_write(ipc_t *ipc, const void *buf, size_t size)
{
    return pipe_write(ipc->server_write_pipe, buf, size);
}

void ipc_client_close_channel(ipc_t *ipc)
{
    bool r_fullyclosed = pipe_close_one_end(ipc->client_read_pipe);
    bool w_fullyclosed = pipe_close_one_end(ipc->client_write_pipe);
    MOS_ASSERT(r_fullyclosed == w_fullyclosed); // both ends should have the same return value

    if (r_fullyclosed || w_fullyclosed)
    {
        // now we can free the ipc
        kfree(ipc->server_name);
        kfree(ipc);
        return;
    }
}

void ipc_server_close_channel(ipc_t *ipc)
{
    bool r_fullyclosed = pipe_close_one_end(ipc->server_read_pipe);
    bool w_fullyclosed = pipe_close_one_end(ipc->server_write_pipe);
    MOS_ASSERT(r_fullyclosed == w_fullyclosed); // both ends should have the same return value

    if (r_fullyclosed || w_fullyclosed)
    {
        // now we can free the ipc
        kfree(ipc->server_name);
        kfree(ipc);
        return;
    }
}

void ipc_init(void)
{
    hashmap_init(&name_waitlist, 128, hashmap_hash_string, hashmap_compare_string);
}

static inode_t *ipc_sysfs_create_ino(ipc_server_t *ipc_server);

ipc_server_t *ipc_server_create(const char *name, size_t max_pending)
{
    pr_dinfo(ipc, "creating ipc server '%s' with max_pending=%zu", name, max_pending);
    spinlock_acquire(&ipc_lock);
    list_foreach(ipc_server_t, server, ipc_servers)
    {
        if (strcmp(server->name, name) == 0)
        {
            spinlock_release(&ipc_lock);
            pr_dwarn(ipc, "ipc server '%s' already exists", name);
            return ERR_PTR(-EEXIST);
        }
    }

    ipc_server_t *server = kmalloc(ipc_server_slab);
    // we don't need to acquire the lock here because the server is not yet announced
    linked_list_init(list_node(server));
    linked_list_init(&server->pending);
    linked_list_init(&server->established);
    waitlist_init(&server->server_waitlist);
    server->name = strdup(name);
    server->pending_max = max_pending;

    // now announce the server
    list_node_append(&ipc_servers, list_node(server));
    ipc_sysfs_create_ino(server);

    // check and see if there is a waitlist for this name
    // if so, wake up all waiters
    waitlist_t *waitlist = hashmap_get(&name_waitlist, (ptr_t) name);
    if (waitlist)
    {
        pr_dinfo2(ipc, "found waitlist for ipc server '%s'", name);
        // wake up all waiters
        waitlist_close(waitlist);
        const size_t n = waitlist_wake_all(waitlist);
        if (n)
            pr_dinfo2(ipc, "woken up %zu waiters for ipc server '%s'", n, name);
    }
    spinlock_release(&ipc_lock);

    return server;
}

ipc_server_t *ipc_get_server(const char *name)
{
    spinlock_acquire(&ipc_lock);
    list_foreach(ipc_server_t, server, ipc_servers)
    {
        if (strcmp(server->name, name) == 0)
        {
            spinlock_release(&ipc_lock);
            return server;
        }
    }

    spinlock_release(&ipc_lock);
    return NULL;
}

ipc_t *ipc_server_accept(ipc_server_t *ipc_server)
{
    pr_dinfo(ipc, "accepting connection on ipc server '%s'...", ipc_server->name);

retry_accept:
    spinlock_acquire(&ipc_server->lock);

    // check if the server is closed
    if (ipc_server->pending_max == 0)
    {
        // now we can free the server
        pr_dinfo2(ipc, "ipc server '%s' is closed, aborting accept()", ipc_server->name);
        kfree(ipc_server->name);
        kfree(ipc_server);
        spinlock_release(&ipc_server->lock);
        return ERR_PTR(-ECONNABORTED);
    }

    if (ipc_server->pending_n == 0)
    {
        // no pending connections, wait for a client to connect
        pr_dinfo2(ipc, "no pending connections, waiting for a client to connect...");
        MOS_ASSERT(waitlist_append(&ipc_server->server_waitlist));
        spinlock_release(&ipc_server->lock);
        blocked_reschedule();
        goto retry_accept; // the server has woken us up, try again
    }

    // get the first pending connection
    MOS_ASSERT(!list_is_empty(&ipc_server->pending));
    ipc_t *ipc = list_node_next_entry(&ipc_server->pending, ipc_t);
    list_remove(ipc);
    ipc_server->pending_n--;
    spinlock_release(&ipc_server->lock);

    MOS_ASSERT(ipc->buffer_size_npages > 0);
    pr_dinfo(ipc, "accepted a connection on ipc server '%s' with buffer_size_npages=%zu", ipc_server->name, ipc->buffer_size_npages);

    // setup the pipes
    ipc->server_read_pipe = pipe_create(ipc->buffer_size_npages);
    ipc->server_write_pipe = pipe_create(ipc->buffer_size_npages);

    // wake up the client
    waitlist_wake_all(&ipc->client_waitlist);

    return ipc;
}

ipc_t *ipc_connect_to_server(const char *name, size_t buffer_size)
{
    if (buffer_size == 0)
        return ERR_PTR(-EINVAL); // buffer size must be > 0

    pr_dinfo(ipc, "connecting to ipc server '%s' with buffer_size=%zu", name, buffer_size);

check_server:
    // check if the server exists
    spinlock_acquire(&ipc_lock);
    ipc_server_t *ipc_server = NULL;
    list_foreach(ipc_server_t, server, ipc_servers)
    {
        if (strcmp(server->name, name) == 0)
        {
            ipc_server = server;
            // we are holding the ipc_servers_lock, so that the server won't deannounce itself
            // while we are checking the server list, thus the server won't be freed
            spinlock_acquire(&ipc_server->lock);
            pr_dinfo2(ipc, "found ipc server '%s'", ipc_server->name);
            break;
        }
    }

    // now we have a server, we can create the connection
    ipc_t *const ipc = kmalloc(ipc_slab);
    linked_list_init(list_node(ipc));
    waitlist_init(&ipc->client_waitlist);
    buffer_size = ALIGN_UP_TO_PAGE(buffer_size);
    ipc->buffer_size_npages = buffer_size / MOS_PAGE_SIZE;
    ipc->server_name = strdup(name);

    if (!ipc_server)
    {
        // no server found, wait for it to be created
        waitlist_t *waitlist = hashmap_get(&name_waitlist, (ptr_t) name);
        if (!waitlist)
        {
            waitlist = kmalloc(waitlist_slab);
            waitlist_init(waitlist);
            hashmap_put(&name_waitlist, (ptr_t) ipc->server_name, waitlist); // the key must be in kernel memory
            pr_dinfo2(ipc, "created waitlist for ipc server '%s'", name);
        }

        pr_dinfo2(ipc, "no ipc server '%s' found, waiting for it to be created...", name);
        MOS_ASSERT(waitlist_append(waitlist));
        spinlock_release(&ipc_lock);
        blocked_reschedule();

        if (signal_has_pending())
        {
            pr_dinfo2(ipc, "woken up by a signal, aborting connect()");
            return ERR_PTR(-EINTR);
        }

        // now check if the server exists again
        goto check_server;
    }
    spinlock_release(&ipc_lock);

    // add the connection to the pending list
    if (ipc_server->pending_n >= ipc_server->pending_max)
    {
        pr_dwarn(ipc, "ipc server '%s' has reached its max pending connections, rejecting connection", ipc_server->name);
        spinlock_release(&ipc_server->lock);
        kfree(ipc);
        return ERR_PTR(-ECONNREFUSED);
    }

    list_node_append(&ipc_server->pending, list_node(ipc)); // add to pending list
    ipc_server->pending_n++;

    // now wait for the server to accept the connection
    MOS_ASSERT(waitlist_append(&ipc->client_waitlist));
    waitlist_wake(&ipc_server->server_waitlist, 1);
    spinlock_release(&ipc_server->lock); // now the server can do whatever it wants

    blocked_reschedule();
    // the server has woken us up and has accepted the connection, or it is closed
    pr_dinfo2(ipc, "ipc server '%s' woke us up", ipc_server->name);

    // check if the server has closed
    if (ipc->buffer_size_npages == 0)
    {
        // the server is closed, don't touch ipc_server pointer anymore
        pr_dwarn(ipc, "ipc server '%s' has closed", ipc_server->name);
        ipc_server = NULL;
        kfree(ipc);
        return ERR_PTR(-ECONNREFUSED);
    }

    // now we have a connection, both the read and write pipes are ready, the io object is also ready
    // we just need to return the io object
    pr_dinfo2(ipc, "ipc server '%s' has accepted the connection", ipc_server->name);
    return ipc;
}

// ! sysfs support

static bool ipc_sysfs_servers(sysfs_file_t *f)
{
    sysfs_printf(f, "%-20s\t%s\n", "Server Name", "Max Pending Connections");
    list_foreach(ipc_server_t, ipc, ipc_servers)
    {
        sysfs_printf(f, "%-20s\t%zu\n", ipc->name, ipc->pending_max);
    }

    return true;
}

static inode_t *ipc_sysfs_create_ino(ipc_server_t *ipc_server)
{
    extern const file_ops_t ipc_sysfs_file_ops;
    ipc_server->sysfs_ino = sysfs_create_inode(FILE_TYPE_CHAR_DEVICE, ipc_server);
    ipc_server->sysfs_ino->perm = PERM_OWNER & (PERM_READ | PERM_WRITE);
    ipc_server->sysfs_ino->file_ops = &ipc_sysfs_file_ops;
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

        MOS_ASSERT(ipc_server->sysfs_ino);
        const size_t w = op(state, ipc_server->sysfs_ino->ino, ipc_server->name, strlen(ipc_server->name), ipc_server->sysfs_ino->type);
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
        if (strcmp(ipc->name, name) == 0)
        {
            ipc_server = ipc;
            break;
        }
    }

    if (ipc_server == NULL)
        return false;

    dentry->inode = ipc_server->sysfs_ino;
    return dentry->inode != NULL;
}

static bool ipc_sysfs_create_server(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(perm);

    if (type != FILE_TYPE_REGULAR)
        return false;

    ipc_server_t *ipc_server = ipc_server_create(dentry->name, 1);
    if (IS_ERR(ipc_server))
        return false;

    dentry->inode = ipc_server->sysfs_ino;
    return true;
}

static bool ipc_dump_name_waitlist(uintn key, void *value, void *data)
{
    MOS_UNUSED(key);
    waitlist_t *waitlist = value;
    sysfs_file_t *f = data;

    spinlock_acquire(&waitlist->lock);
    sysfs_printf(f, "%s\t%s:\n", (const char *) key, waitlist->closed ? "closed" : "open");
    list_foreach(thread_t, thread, waitlist->list)
    {
        sysfs_printf(f, "\t%s\n", thread->name);
    }
    spinlock_release(&waitlist->lock);

    return true;
}

static bool ipc_sysfs_dump_name_waitlist(sysfs_file_t *f)
{
    sysfs_printf(f, "%-20s\t%s\n", "IPC Name", "Status");
    spinlock_acquire(&ipc_lock);
    hashmap_foreach(&name_waitlist, ipc_dump_name_waitlist, f);
    spinlock_release(&ipc_lock);
    return true;
}

static sysfs_item_t ipc_sysfs_items[] = {
    SYSFS_RO_ITEM("servers", ipc_sysfs_servers),
    SYSFS_DYN_DIR("ipcs", ipc_sysfs_list_ipcs, ipc_sysfs_lookup_ipc, ipc_sysfs_create_server),
    SYSFS_RO_ITEM("name_waitlist", ipc_sysfs_dump_name_waitlist),
};

SYSFS_AUTOREGISTER(ipc, ipc_sysfs_items);
