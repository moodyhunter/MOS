// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ipc/ipc.hpp"

#include "mos/filesystem/dentry.hpp"
#include "mos/filesystem/inode.hpp"
#include "mos/filesystem/sysfs/sysfs.hpp"
#include "mos/filesystem/sysfs/sysfs_autoinit.hpp"
#include "mos/filesystem/vfs_types.hpp"
#include "mos/filesystem/vfs_utils.hpp"
#include "mos/ipc/pipe.hpp"
#include "mos/lib/sync/spinlock.hpp"
#include "mos/tasks/schedule.hpp"
#include "mos/tasks/signal.hpp"
#include "mos/tasks/thread.hpp"
#include "mos/tasks/wait.hpp"

#include <mos/allocator.hpp>
#include <mos/filesystem/fs_types.h>
#include <mos/hashmap.hpp>
#include <mos/lib/structures/hashmap_common.hpp>
#include <mos/lib/structures/list.hpp>
#include <mos/mos_global.h>
#include <mos/string.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>

#define IPC_SERVER_MAGIC MOS_FOURCC('I', 'P', 'C', 'S')

struct IpcDescriptor final : mos::NamedType<"IPC.Descriptor">
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

    IpcDescriptor(mos::string_view name, size_t buffer_size) //
        : server_name(name),                                 //
          buffer_size_npages(buffer_size / MOS_PAGE_SIZE)    //
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

    IPCServer(mos::string_view name, size_t pending_max) : name(name), pending_max(pending_max)
    {
        linked_list_init(list_node(this));
        linked_list_init(&pending);
    }

    ~IPCServer()
    {
        spinlock_release(&lock);
    }
};

// protects ipc_servers and name_waitlist
static spinlock_t ipc_lock;

static list_head ipc_servers;

// waitlist for an IPC server, key = name, value = waitlist_t *
static mos::HashMap<mos::string, waitlist_t *> name_waitlist;

void ipc_server_close(IPCServer *server)
{
    spinlock_acquire(&ipc_lock);
    spinlock_acquire(&server->lock);
    // remove the server from the list
    list_remove(server);

    if (const auto &it = name_waitlist.find(server->name); it)
    {
        const auto waitlist = it.value();
        // the waitlist should have been closed when the server was created
        // and no one should be waiting on it
        spinlock_acquire(&waitlist->lock);
        MOS_ASSERT(waitlist->closed);
        MOS_ASSERT(waitlist->waiters.empty());
        spinlock_release(&waitlist->lock);
        waitlist->reset(); // reuse the waitlist
    }
    spinlock_release(&ipc_lock);

    // with the server lock held, we reject all pending connections
    list_foreach(IpcDescriptor, ipc, server->pending)
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
        dentry_t *ipc_get_sysfs_dir(void);
        // now we can free the server
        const auto dparent = ipc_get_sysfs_dir();

        const auto dentry = dentry_get_from_parent(server->sysfs_ino->superblock, dparent, server->name);
        if (dentry->inode == nullptr)
            dentry_attach(dentry, server->sysfs_ino); // fixup, as lookup may not have been called
        inode_unlink(server->sysfs_ino, dentry);
        dentry_unref(dentry); // it won't release dentry because dentry->inode is still valid
        dentry_detach(dentry);
        dentry_try_release(dentry);
        server->sysfs_ino = NULL;
        delete server;
    }
}

size_t ipc_client_read(IpcDescriptor *ipc, void *buf, size_t size)
{
    return pipe_read(ipc->client_read_pipe, buf, size);
}

size_t ipc_client_write(IpcDescriptor *ipc, const void *buf, size_t size)
{
    return pipe_write(ipc->client_write_pipe, buf, size);
}

size_t ipc_server_read(IpcDescriptor *ipc, void *buf, size_t size)
{
    return pipe_read(ipc->server_read_pipe, buf, size);
}

size_t ipc_server_write(IpcDescriptor *ipc, const void *buf, size_t size)
{
    return pipe_write(ipc->server_write_pipe, buf, size);
}

void ipc_client_close_channel(IpcDescriptor *ipc)
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

void ipc_server_close_channel(IpcDescriptor *ipc)
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

static inode_t *ipc_sysfs_create_ino(IPCServer *ipc_server);

PtrResult<IPCServer> ipc_server_create(mos::string_view name, size_t max_pending)
{
    dInfo<ipc> << "creating ipc server '" << name << "' with max_pending=" << max_pending;
    const auto guard = ipc_lock.lock();
    list_foreach(IPCServer, server, ipc_servers)
    {
        if (server->name == name)
        {
            dWarn<ipc> << "ipc server '" << name << "' already exists";
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
    if (const auto it = name_waitlist.find(name); it)
    {
        auto waitlist = it.value();
        dInfo<ipc> << "found waitlist for ipc server '" << name << "'";
        // wake up all waiters
        waitlist_close(waitlist);
        const size_t n = waitlist_wake_all(&*waitlist);
        if (n)
            dInfo<ipc> << "woken up " << n << " waiters for ipc server '" << name << "'";
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

PtrResult<IpcDescriptor> ipc_server_accept(IPCServer *ipc_server)
{
    dInfo<ipc> << "accepting connection on ipc server '" << ipc_server->name << "'...";

retry_accept:
    spinlock_acquire(&ipc_server->lock);

    // check if the server is closed
    if (ipc_server->pending_max == 0)
    {
        // now we can free the server
        dInfo<ipc> << "ipc server '" << ipc_server->name << "' is closed, aborting accept()";
        delete ipc_server;
        return -ECONNABORTED;
    }

    if (ipc_server->pending_n == 0)
    {
        // no pending connections, wait for a client to connect
        dInfo<ipc> << "no pending connections, waiting for a client to connect...";
        MOS_ASSERT(waitlist_append(&ipc_server->server_waitlist));
        spinlock_release(&ipc_server->lock);
        blocked_reschedule();

        if (signal_has_pending())
        {
            dInfo<ipc> << "woken up by a signal, aborting accept()";
            waitlist_remove_me(&ipc_server->server_waitlist);
            return -EINTR;
        }

        goto retry_accept; // the server has woken us up, try again
    }

    // get the first pending connection
    MOS_ASSERT(!list_is_empty(&ipc_server->pending));
    IpcDescriptor *desc = list_node_next_entry(&ipc_server->pending, IpcDescriptor);
    list_remove(desc);
    ipc_server->pending_n--;
    spinlock_release(&ipc_server->lock);

    MOS_ASSERT(desc->buffer_size_npages > 0);
    dInfo<ipc> << "accepted a connection on ipc server '" << ipc_server->name << "' with buffer_size_npages=" << desc->buffer_size_npages;

    // setup the pipes
    auto readPipe = pipe_create(desc->buffer_size_npages);
    if (readPipe.isErr())
    {
        dWarn<ipc> << "failed to create read pipe";
        // TODO: cleanup
        return readPipe.getErr();
    }
    desc->server_read_pipe = readPipe.get();

    auto writePipe = pipe_create(desc->buffer_size_npages);
    if (writePipe.isErr())
    {
        dWarn<ipc> << "failed to create write pipe";
        // TODO: cleanup
        return writePipe.getErr();
    }
    desc->server_write_pipe = writePipe.get();

    // wake up the client
    waitlist_wake_all(&desc->client_waitlist);

    return desc;
}

PtrResult<IpcDescriptor> ipc_connect_to_server(mos::string name, size_t buffer_size)
{
    if (buffer_size == 0)
        return -EINVAL; // buffer size must be > 0

    dInfo<ipc> << "connecting to ipc server '" << name << "' with buffer_size=" << buffer_size;
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
            dInfo<ipc> << "found ipc server '" << ipc_server->name << "'";
            break;
        }
    }

    // now we have a server, we can create the connection
    const auto descriptor = mos::create<IpcDescriptor>(name, buffer_size);

    if (!ipc_server)
    {
        // no server found, wait for it to be created
        waitlist_t *waitlist;
        if (const auto it = name_waitlist.find(name); it)
        {
            waitlist = it.value();
            dInfo<ipc> << "found existing waitlist for ipc server '" << name << "'";
        }
        else
        {
            waitlist = mos::create<waitlist_t>();
            // the key must be in kernel memory
            name_waitlist.insert(descriptor->server_name, waitlist);
            dInfo<ipc> << "created waitlist for ipc server '" << name << "'";
        }

        dInfo<ipc> << "no ipc server '" << name << "' found, waiting for it to be created...";
        MOS_ASSERT(waitlist_append(waitlist));
        spinlock_release(&ipc_lock);
        blocked_reschedule();

        if (signal_has_pending())
        {
            dInfo<ipc> << "woken up by a signal, aborting connect()";
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
        dWarn<ipc> << "ipc server '" << ipc_server->name << "' has reached its max pending connections, rejecting connection";
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
    dInfo<ipc> << "ipc server '" << ipc_server->name << "' woke us up";

    // check if the server has closed
    if (descriptor->buffer_size_npages == 0)
    {
        // the server is closed, don't touch ipc_server pointer anymore
        dWarn<ipc> << "ipc server '" << ipc_server->name << "' has closed";
        ipc_server = NULL;
        delete descriptor;
        return -ECONNREFUSED;
    }

    // now we have a connection, both the read and write pipes are ready, the io object is also ready
    // we just need to return the io object
    dInfo<ipc> << "ipc server '" << ipc_server->name << "' has accepted the connection";
    return descriptor;
}

// ! sysfs support

static bool ipc_sysfs_servers(sysfs_file_t *f)
{
    sysfs_printf(f, "%-40s\t%s\n", "Server Name", "Max Pending Connections");
    list_foreach(IPCServer, ipc, ipc_servers)
    {
        sysfs_printf(f, "%-40s\t%zu\n", ipc->name.c_str(), ipc->pending_max);
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

static bool ipc_sysfs_dump_name_waitlist(sysfs_file_t *f)
{
    sysfs_printf(f, "%-20s\t%s\n", "IPC Name", "Status");
    const auto guard = ipc_lock.lock();
    for (const auto &[name, waitlist] : name_waitlist)
    {
        sysfs_printf(f, "%-20s\t%s:\n", name.c_str(), waitlist->closed ? "closed" : "open");
        for (const auto tid : waitlist->waiters)
        {
            const auto thread = thread_get(tid);
            sysfs_printf(f, "\t%s\n", thread->name.c_str());
        }
    }
    return true;
}

static sysfs_item_t ipc_sysfs_items[] = {
    SYSFS_RO_ITEM("servers", ipc_sysfs_servers),
    SYSFS_DYN_DIR("ipcs", ipc_sysfs_list_ipcs, ipc_sysfs_lookup_ipc, ipc_sysfs_create_server),
    SYSFS_RO_ITEM("name_waitlist", ipc_sysfs_dump_name_waitlist),
};

SYSFS_AUTOREGISTER(ipc, ipc_sysfs_items);

dentry_t *ipc_get_sysfs_dir()
{
    return __sysfs_ipc._dentry;
}
