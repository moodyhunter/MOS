// SPDX-License-Identifier: GPL-3.0-or-later

#define pr_fmt(fmt) "ipc: " fmt

#include "mos/ipc/ipc.hpp"

#include "mos/filesystem/dentry.hpp"
#include "mos/filesystem/sysfs/sysfs.hpp"
#include "mos/filesystem/sysfs/sysfs_autoinit.hpp"
#include "mos/filesystem/vfs_types.hpp"
#include "mos/ipc/pipe.hpp"
#include "mos/lib/sync/spinlock.hpp"
#include "mos/platform/platform.hpp"
#include "mos/syslog/printk.hpp"
#include "mos/tasks/schedule.hpp"
#include "mos/tasks/signal.hpp"
#include "mos/tasks/wait.hpp"

#include <mos/allocator.hpp>
#include <mos/filesystem/fs_types.h>
#include <mos/lib/structures/hashmap_common.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/mos_global.h>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

#define IPC_SERVER_MAGIC MOS_FOURCC('I', 'P', 'C', 'S')

struct IPCDescriptor final : mos::NamedType<"IPC.Descriptor">
{
    as_linked_list; ///< attached to either pending or established list
    const mos::string server_name;
    size_t buffer_size_npages;

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

    IPCDescriptor(mos::string_view name, size_t buffer_size) : server_name(name), buffer_size_npages(buffer_size / MOS_PAGE_SIZE)
    {
    }
};

struct IPCServer final : public mos::NamedType<"IPCServer">
{
    as_linked_list;
    const mos::string name;
    spinlock_t lock;
    inode_t *sysfs_ino; ///< inode for sysfs
    size_t pending_max, pending_n;
    size_t established_n;
    list_head pending; ///< list of IPCDescriptor

    waitlist_t server_waitlist; ///< wake up the server here when a client connects

    void *key() const
    {
        return (void *) name.c_str();
    }

    IPCServer(mos::string_view name, size_t pending_max) : name(name), pending_max(pending_max)
    {
        linked_list_init(list_node(this));
        linked_list_init(&pending);
        waitlist_init(&server_waitlist);
    }

    ~IPCServer()
    {
        spinlock_release(&lock);
    }
};

static list_head ipc_servers;
static hashmap_t name_waitlist; ///< waitlist for an IPC server, key = name, value = waitlist_t *
static spinlock_t ipc_lock;     ///< protects ipc_servers and name_waitlist

void ipc_server_close(IPCServer *server)
{
    spinlock_acquire(&ipc_lock);
    spinlock_acquire(&server->lock);
    // remove the server from the list
    list_remove(server);

    waitlist_t *waitlist = (waitlist_t *) hashmap_get(&name_waitlist, (ptr_t) server->key());
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
    list_foreach(IPCDescriptor, ipc, server->pending)
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
        delete server;
    }
}

size_t ipc_client_read(IPCDescriptor *ipc, void *buf, size_t size)
{
    return pipe_read(ipc->client_read_pipe, buf, size);
}

size_t ipc_client_write(IPCDescriptor *ipc, const void *buf, size_t size)
{
    return pipe_write(ipc->client_write_pipe, buf, size);
}

size_t ipc_server_read(IPCDescriptor *ipc, void *buf, size_t size)
{
    return pipe_read(ipc->server_read_pipe, buf, size);
}

size_t ipc_server_write(IPCDescriptor *ipc, const void *buf, size_t size)
{
    return pipe_write(ipc->server_write_pipe, buf, size);
}

void ipc_client_close_channel(IPCDescriptor *ipc)
{
    bool r_fullyclosed = pipe_close_one_end(ipc->client_read_pipe);
    bool w_fullyclosed = pipe_close_one_end(ipc->client_write_pipe);
    MOS_ASSERT(r_fullyclosed == w_fullyclosed); // both ends should have the same return value

    if (r_fullyclosed || w_fullyclosed)
    {
        // now we can free the ipc
        delete ipc;
        return;
    }
}

void ipc_server_close_channel(IPCDescriptor *ipc)
{
    bool r_fullyclosed = pipe_close_one_end(ipc->server_read_pipe);
    bool w_fullyclosed = pipe_close_one_end(ipc->server_write_pipe);
    MOS_ASSERT(r_fullyclosed == w_fullyclosed); // both ends should have the same return value

    if (r_fullyclosed || w_fullyclosed)
    {
        // now we can free the ipc
        delete ipc;
        return;
    }
}

void ipc_init(void)
{
    hashmap_init(&name_waitlist, 128, hashmap_hash_string, hashmap_compare_string);
}

static inode_t *ipc_sysfs_create_ino(IPCServer *ipc_server);

PtrResult<IPCServer> ipc_server_create(mos::string_view name, size_t max_pending)
{
    pr_dinfo(ipc, "creating ipc server '%s' with max_pending=%zu", name.data(), max_pending);
    const auto guard = ipc_lock.lock();
    list_foreach(IPCServer, server, ipc_servers)
    {
        if (server->name == name)
        {
            pr_dwarn(ipc, "ipc server '%s' already exists", name.data());
            return -EEXIST;
        }
    }

    // we don't need to acquire the lock here because the server is not yet announced
    const auto server = mos::create<IPCServer>(name, max_pending);

    // now announce the server
    list_node_append(&ipc_servers, list_node(server));
    ipc_sysfs_create_ino(server);

    // check and see if there is a waitlist for this name
    // if so, wake up all waiters
    waitlist_t *waitlist = (waitlist_t *) hashmap_get(&name_waitlist, (ptr_t) server->key());
    if (waitlist)
    {
        pr_dinfo2(ipc, "found waitlist for ipc server '%s'", name.data());
        // wake up all waiters
        waitlist_close(waitlist);
        const size_t n = waitlist_wake_all(waitlist);
        if (n)
            pr_dinfo2(ipc, "woken up %zu waiters for ipc server '%s'", n, name.data());
    }

    return server;
}

PtrResult<IPCServer> ipc_get_server(mos::string_view name)
{
    const auto guard = ipc_lock.lock();
    list_foreach(IPCServer, server, ipc_servers)
    {
        if (server->name == name)
            return server;
    }

    return -ENOENT;
}

PtrResult<IPCDescriptor> ipc_server_accept(IPCServer *ipc_server)
{
    pr_dinfo(ipc, "accepting connection on ipc server '%s'...", ipc_server->name.c_str());

retry_accept:
    spinlock_acquire(&ipc_server->lock);

    // check if the server is closed
    if (ipc_server->pending_max == 0)
    {
        // now we can free the server
        pr_dinfo2(ipc, "ipc server '%s' is closed, aborting accept()", ipc_server->name.c_str());
        delete ipc_server;
        return -ECONNABORTED;
    }

    if (ipc_server->pending_n == 0)
    {
        // no pending connections, wait for a client to connect
        pr_dinfo2(ipc, "no pending connections, waiting for a client to connect...");
        MOS_ASSERT(waitlist_append(&ipc_server->server_waitlist));
        spinlock_release(&ipc_server->lock);
        blocked_reschedule();

        if (signal_has_pending())
        {
            pr_dinfo2(ipc, "woken up by a signal, aborting accept()");
            waitlist_remove_me(&ipc_server->server_waitlist);
            return -EINTR;
        }

        goto retry_accept; // the server has woken us up, try again
    }

    // get the first pending connection
    MOS_ASSERT(!list_is_empty(&ipc_server->pending));
    IPCDescriptor *ipc = list_node_next_entry(&ipc_server->pending, IPCDescriptor);
    list_remove(ipc);
    ipc_server->pending_n--;
    spinlock_release(&ipc_server->lock);

    MOS_ASSERT(ipc->buffer_size_npages > 0);
    pr_dinfo(ipc, "accepted a connection on ipc server '%s' with buffer_size_npages=%zu", ipc_server->name.c_str(), ipc->buffer_size_npages);

    // setup the pipes
    auto readPipe = pipe_create(ipc->buffer_size_npages);
    if (readPipe.isErr())
    {
        pr_dwarn(ipc, "failed to create read pipe");
        // TODO: cleanup
        return readPipe.getErr();
    }
    ipc->server_read_pipe = readPipe.get();

    auto writePipe = pipe_create(ipc->buffer_size_npages);
    if (writePipe.isErr())
    {
        pr_dwarn(ipc, "failed to create write pipe");
        // TODO: cleanup
        return writePipe.getErr();
    }
    ipc->server_write_pipe = writePipe.get();

    // wake up the client
    waitlist_wake_all(&ipc->client_waitlist);

    return ipc;
}

PtrResult<IPCDescriptor> ipc_connect_to_server(mos::string_view name, size_t buffer_size)
{
    if (buffer_size == 0)
        return -EINVAL; // buffer size must be > 0

    pr_dinfo(ipc, "connecting to ipc server '%s' with buffer_size=%zu", name.data(), buffer_size);
    buffer_size = ALIGN_UP_TO_PAGE(buffer_size);

check_server:
    // check if the server exists
    spinlock_acquire(&ipc_lock);
    IPCServer *ipc_server = NULL;
    list_foreach(IPCServer, server, ipc_servers)
    {
        if (server->name == name)
        {
            ipc_server = server;
            // we are holding the ipc_servers_lock, so that the server won't deannounce itself
            // while we are checking the server list, thus the server won't be freed
            spinlock_acquire(&ipc_server->lock);
            pr_dinfo2(ipc, "found ipc server '%s'", ipc_server->name.c_str());
            break;
        }
    }

    // now we have a server, we can create the connection
    const auto descriptor = mos::create<IPCDescriptor>(name, buffer_size);

    if (!ipc_server)
    {
        // no server found, wait for it to be created
        waitlist_t *waitlist = (waitlist_t *) hashmap_get(&name_waitlist, (ptr_t) name.data());
        if (!waitlist)
        {
            waitlist = mos::create<waitlist_t>();
            waitlist_init(waitlist);
            // the key must be in kernel memory
            const auto old = (waitlist_t *) hashmap_put(&name_waitlist, (ptr_t) descriptor->server_name.c_str(), waitlist);
            if (old)
            {
                // someone else has created the waitlist, but now we have replaced it
                // so we have to append the old waitlist to the new one
                list_foreach(Thread, thread, old->list)
                {
                    MOS_ASSERT(waitlist_append(waitlist));
                }
            }
            pr_dinfo2(ipc, "created waitlist for ipc server '%s'", name.data());
        }

        pr_dinfo2(ipc, "no ipc server '%s' found, waiting for it to be created...", name.data());
        MOS_ASSERT(waitlist_append(waitlist));
        spinlock_release(&ipc_lock);
        blocked_reschedule();

        if (signal_has_pending())
        {
            pr_dinfo2(ipc, "woken up by a signal, aborting connect()");
            delete descriptor;
            return -EINTR;
        }

        // now check if the server exists again
        goto check_server;
    }
    spinlock_release(&ipc_lock);

    // add the connection to the pending list
    if (ipc_server->pending_n >= ipc_server->pending_max)
    {
        pr_dwarn(ipc, "ipc server '%s' has reached its max pending connections, rejecting connection", ipc_server->name.c_str());
        spinlock_release(&ipc_server->lock);
        delete descriptor;
        return -ECONNREFUSED;
    }

    list_node_append(&ipc_server->pending, list_node(descriptor)); // add to pending list
    ipc_server->pending_n++;

    // now wait for the server to accept the connection
    MOS_ASSERT(waitlist_append(&descriptor->client_waitlist));
    waitlist_wake(&ipc_server->server_waitlist, 1);
    spinlock_release(&ipc_server->lock); // now the server can do whatever it wants

    blocked_reschedule();
    // the server has woken us up and has accepted the connection, or it is closed
    pr_dinfo2(ipc, "ipc server '%s' woke us up", ipc_server->name.c_str());

    // check if the server has closed
    if (descriptor->buffer_size_npages == 0)
    {
        // the server is closed, don't touch ipc_server pointer anymore
        pr_dwarn(ipc, "ipc server '%s' has closed", ipc_server->name.c_str());
        ipc_server = NULL;
        delete descriptor;
        return -ECONNREFUSED;
    }

    // now we have a connection, both the read and write pipes are ready, the io object is also ready
    // we just need to return the io object
    pr_dinfo2(ipc, "ipc server '%s' has accepted the connection", ipc_server->name.c_str());
    return descriptor;
}

// ! sysfs support

static bool ipc_sysfs_servers(sysfs_file_t *f)
{
    sysfs_printf(f, "%-20s\t%s\n", "Server Name", "Max Pending Connections");
    list_foreach(IPCServer, ipc, ipc_servers)
    {
        sysfs_printf(f, "%-20s\t%zu\n", ipc->name.c_str(), ipc->pending_max);
    }

    return true;
}

static inode_t *ipc_sysfs_create_ino(IPCServer *ipc_server)
{
    ipc_server->sysfs_ino = sysfs_create_inode(FILE_TYPE_CHAR_DEVICE, ipc_server);
    ipc_server->sysfs_ino->perm = PERM_OWNER & (PERM_READ | PERM_WRITE);
    ipc_server->sysfs_ino->file_ops = &ipc_sysfs_file_ops;
    return ipc_server->sysfs_ino;
}

static void ipc_sysfs_list_ipcs(sysfs_item_t *item, dentry_t *d, vfs_listdir_state_t *state, dentry_iterator_op add_record)
{
    MOS_UNUSED(item);
    MOS_UNUSED(d);

    list_foreach(IPCServer, ipc_server, ipc_servers)
    {
        MOS_ASSERT(ipc_server->sysfs_ino);
        add_record(state, ipc_server->sysfs_ino->ino, ipc_server->name, ipc_server->sysfs_ino->type);
    }
}

static bool ipc_sysfs_lookup_ipc(inode_t *parent_dir, dentry_t *dentry)
{
    MOS_UNUSED(parent_dir);

    const auto name = dentry->name;
    IPCServer *ipc_server = NULL;
    list_foreach(IPCServer, ipc, ipc_servers)
    {
        if (ipc->name == name)
        {
            ipc_server = ipc;
            break;
        }
    }

    if (ipc_server == NULL)
        return false;

    dentry_attach(dentry, ipc_server->sysfs_ino);
    return dentry->inode != NULL;
}

static bool ipc_sysfs_create_server(inode_t *dir, dentry_t *dentry, file_type_t type, file_perm_t perm)
{
    MOS_UNUSED(dir);
    MOS_UNUSED(perm);

    if (type != FILE_TYPE_REGULAR)
        return false;

    auto ipc_server = ipc_server_create(dentry->name, 1);
    if (ipc_server.isErr())
        return false;

    dentry_attach(dentry, ipc_server->sysfs_ino);
    return true;
}

static bool ipc_dump_name_waitlist(uintn key, void *value, void *data)
{
    MOS_UNUSED(key);
    waitlist_t *waitlist = (waitlist_t *) value;
    sysfs_file_t *f = (sysfs_file_t *) data;

    const auto guard = waitlist->lock.lock();
    sysfs_printf(f, "%s\t%s:\n", (const char *) key, waitlist->closed ? "closed" : "open");
    list_foreach(Thread, thread, waitlist->list)
    {
        sysfs_printf(f, "\t%s\n", thread->name.c_str());
    }

    return true;
}

static bool ipc_sysfs_dump_name_waitlist(sysfs_file_t *f)
{
    sysfs_printf(f, "%-20s\t%s\n", "IPC Name", "Status");
    const auto guard = ipc_lock.lock();
    hashmap_foreach(&name_waitlist, ipc_dump_name_waitlist, f);
    return true;
}

static sysfs_item_t ipc_sysfs_items[] = {
    SYSFS_RO_ITEM("servers", ipc_sysfs_servers),
    SYSFS_DYN_DIR("ipcs", ipc_sysfs_list_ipcs, ipc_sysfs_lookup_ipc, ipc_sysfs_create_server),
    SYSFS_RO_ITEM("name_waitlist", ipc_sysfs_dump_name_waitlist),
};

SYSFS_AUTOREGISTER(ipc, ipc_sysfs_items);
