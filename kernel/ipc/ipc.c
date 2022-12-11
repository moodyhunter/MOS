// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ipc/ipc.h"

#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "lib/structures/hashmap_common.h"
#include "lib/structures/ring_buffer.h"
#include "lib/sync/refcount.h"
#include "mos/io/io.h"
#include "mos/ipc/ipc_types.h"
#include "mos/mm/kmalloc.h"
#include "mos/printk.h"
#include "mos/tasks/thread.h"

#define IPC_CHANNEL_HASHMAP_SIZE 64

static hashmap_t *ipc_channels;

static hash_t ipc_channel_hash(const void *key)
{
    return hashmap_hash_string(key);
}

static int ipc_channel_compare(const void *a, const void *b)
{
    ipc_server_t *ca = (ipc_server_t *) a;
    ipc_server_t *cb = (ipc_server_t *) b;
    return strcmp(ca->name, cb->name) == 0;
}

static size_t ipc_server_read(io_t *io, void *buf, size_t count)
{
    MOS_UNUSED(io);
    MOS_UNUSED(buf);
    MOS_UNUSED(count);
    return 0;
}

static size_t ipc_server_write(io_t *io, const void *buf, size_t count)
{
    MOS_UNUSED(io);
    MOS_UNUSED(buf);
    MOS_UNUSED(count);
    return 0;
}

static void ipc_server_close(io_t *io)
{
    pr_info("closing ipc channel (io refcount: %zu)", io->refcount);
    ipc_io_t *ipc_io = container_of(io, ipc_io_t, io);
    ipc_server_t *channel = ipc_io->server;
    hashmap_remove(ipc_channels, channel->name);
    kfree(channel);
}

static io_op_t ipc_ops = {
    .read = ipc_server_read,
    .write = ipc_server_write,
    .close = ipc_server_close,
};

void ipc_init(void)
{
    pr_info("Initializing IPC subsystem...");
    ipc_channels = kmalloc(sizeof(hashmap_t));
    hashmap_init(ipc_channels, IPC_CHANNEL_HASHMAP_SIZE, ipc_channel_hash, ipc_channel_compare);
}

io_t *ipc_open(process_t *owner, vmblock_t vmblock, const char *name, ipc_open_flags flags)
{
    bool create_only = flags & IPC_OPEN_CREATE_ONLY;
    bool existing_only = flags & IPC_OPEN_EXISTING_ONLY;

    if (unlikely(create_only && existing_only))
    {
        pr_warn("IPC channel open flags are invalid");
        return NULL;
    }

    ipc_server_t *channel = hashmap_get(ipc_channels, name);

    if (unlikely(!channel && existing_only))
    {
        pr_warn("IPC channel %s does not exist", name);
        return NULL;
    }

    if (unlikely(channel && create_only))
    {
        pr_warn("IPC channel %s already exists", name);
        return NULL;
    }

    if (!channel)
    {
        return ipc_create_server(owner, vmblock, name);
    }
    else
    {
        return ipc_create_client(vmblock, name);
    }
}

io_t *ipc_create_server(process_t *owner, vmblock_t vmblock, const char *name)
{
    pr_info2("creating new channel %s", name);
    ipc_server_t *channel = kzalloc(sizeof(ipc_server_t));
    channel->name = duplicate_string(name, strlen(name));
    channel->address_space = owner->pagetable;
    channel->vmblock = vmblock;
    hashmap_put(ipc_channels, channel->name, channel);

    ipc_io_t *ipcio = kzalloc(sizeof(ipc_io_t));
    ipcio->server = channel;
    io_init(&ipcio->io, IO_READABLE | IO_WRITABLE, 0, &ipc_ops);
    return &ipcio->io;
}

io_t *ipc_create_client(vmblock_t vmblock, const char *name)
{
    // pr_info2("opening channel %s", name);
    // ipc_server_t *channel = hashmap_get(ipc_channels, name);
    // ipc_io_t *ipcio = kzalloc(sizeof(ipc_io_t));
    // ipcio->server = channel;
    // io_init(&ipcio->io, IO_READABLE | IO_WRITABLE, 0, &ipc_ops);
    // return &ipcio->io;
    MOS_UNUSED(vmblock);
    MOS_UNUSED(name);
    return NULL;
}
