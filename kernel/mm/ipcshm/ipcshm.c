// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/filesystem/ipcfs/ipcfs.h>
#include <mos/lib/structures/hashmap.h>
#include <mos/lib/structures/hashmap_common.h>
#include <mos/lib/sync/spinlock.h>
#include <mos/mm/ipcshm/ipcshm.h>
#include <mos/mm/kmalloc.h>
#include <mos/mm/shm.h>
#include <mos/printk.h>
#include <mos/tasks/schedule.h>
#include <mos/tasks/task_types.h>
#include <mos/tasks/wait.h>
#include <stdlib.h>
#include <string.h>

#define IPCSHM_BILLBOARD_HASHMAP_SIZE 64
#define IPCSHM_SERVER_MAGIC           MOS_FOURCC('I', 'S', 'H', 'M')

static hashmap_t *ipcshm_billboard;
static spinlock_t billboard_lock;

static ipcshm_t *ipcshm_server_get_pending(ipcshm_server_t *server)
{
    ipcshm_t *pending = NULL;
    spinlock_acquire(&server->pending_lock);
    for (size_t i = 0; i < server->max_pending; i++)
    {
        if (server->pending[i]->state == IPCSHM_PENDING)
            pending = server->pending[i];
    }
    spinlock_release(&server->pending_lock);
    return pending;
}

static bool wc_ipcshm_pending_or_closed(wait_condition_t *cond)
{
    ipcshm_server_t *server = cond->arg;
    if (server->magic != IPCSHM_SERVER_MAGIC)
        return true;
    return ipcshm_server_get_pending(server) != NULL;
}

static bool wc_ipcshm_is_attached_or_freed(wait_condition_t *cond)
{
    ipcshm_t *conn = cond->arg;
    return conn->state == IPCSHM_ATTACHED || conn->state == IPCSHM_FREE;
}

static bool wc_ipcshm_server_name_exists(wait_condition_t *cond)
{
    const char *name = cond->arg;
    spinlock_acquire(&billboard_lock);
    ipcshm_server_t *server = hashmap_get(ipcshm_billboard, (uintptr_t) name);
    spinlock_release(&billboard_lock);
    return server != NULL;
}

static void wc_ipcshm_server_name_free(wait_condition_t *cond)
{
    const char *name = cond->arg;
    kfree(name);
}

void ipcshm_init(void)
{
    pr_info("initializing shared-memory IPC backend");
    ipcshm_billboard = kzalloc(sizeof(hashmap_t));
    hashmap_init(ipcshm_billboard, IPCSHM_BILLBOARD_HASHMAP_SIZE, hashmap_hash_string, hashmap_compare_string);
}

ipcshm_server_t *ipcshm_announce(const char *name, size_t max_pending)
{
    spinlock_acquire(&billboard_lock);
    ipcshm_server_t *existing_server = hashmap_get(ipcshm_billboard, (uintptr_t) name);
    spinlock_release(&billboard_lock);

    if (unlikely(existing_server))
    {
        pr_warn("IPC channel '%s' already exists", name);
        return NULL;
    }

    pr_info("ipc: channel '%s' created", name);
    ipcshm_server_t *server = kzalloc(sizeof(ipcshm_server_t));
    server->magic = IPCSHM_SERVER_MAGIC;
    server->name = strdup(name);
    server->max_pending = max_pending;
    server->pending = kzalloc(sizeof(ipcshm_t *) * max_pending);
    for (size_t i = 0; i < max_pending; i++)
        server->pending[i] = kzalloc(sizeof(ipcshm_t));

    spinlock_acquire(&billboard_lock);
    hashmap_put(ipcshm_billboard, (uintptr_t) server->name, server);
    spinlock_release(&billboard_lock);
    ipcfs_register_server(server);
    return server;
}

ipcshm_t *ipcshm_request(const char *name, size_t buffer_size, void **read_buf, void **write_buf, void *data)
{
    pr_info("ipc: connecting to channel '%s'", name);
    buffer_size = ALIGN_UP(buffer_size, MOS_PAGE_SIZE);

    spinlock_acquire(&billboard_lock);
    ipcshm_server_t *server = hashmap_get(ipcshm_billboard, (uintptr_t) name);
    spinlock_release(&billboard_lock);

    if (unlikely(!server))
    {
        mos_debug(ipc, "no server found for channel '%s', waiting...", name);
        reschedule_for_wait_condition(wc_wait_for(strdup(name), wc_ipcshm_server_name_exists, wc_ipcshm_server_name_free));
        mos_debug(ipc, "server for channel '%s' found, connecting...", name);

        spinlock_acquire(&billboard_lock);
        server = hashmap_get(ipcshm_billboard, (uintptr_t) name);
        spinlock_release(&billboard_lock);

        MOS_ASSERT(server);
    }

    mos_debug(ipc, "connecting to channel '%s'", name);

    ipcshm_t *shm = NULL;

    if (server->magic != IPCSHM_SERVER_MAGIC)
    {
        pr_warn("server magic is invalid (0x%x)", server->magic);
        return NULL;
    }

    // find a free pending connection slot, or fail if there are none
    spinlock_acquire(&server->pending_lock);
    for (size_t i = 0; i < server->max_pending; i++)
    {
        spinlock_acquire(&server->pending[i]->lock);
        if (server->pending[i]->state == IPCSHM_FREE)
        {
            shm = server->pending[i];
            shm->state = IPCSHM_PENDING;
            shm->server = server;
            shm->buffer_size = buffer_size;
            break; // keep the shm lock held intentionally
        }
        spinlock_release(&server->pending[i]->lock);
    }
    spinlock_release(&server->pending_lock);

    if (unlikely(!shm))
    {
        pr_warn("no pending connection slots available");
        *read_buf = NULL;
        *write_buf = NULL;
        return NULL;
    }

    // there are 3 steps for a client to connect to a server:
    //
    // 1. client: allocates shm, and sets client_write_buffer
    // 2. client: waits for the server to accept the connection and set server_write_buffer
    // 3. client is woken up, and sets server_write_buffer

    // step 1
    {
        shm->client_write_shm = shm_allocate(shm->buffer_size / MOS_PAGE_SIZE, VMAP_FORK_PRIVATE, VM_USER_RW);
        *write_buf = (void *) shm->client_write_shm.vaddr;
        shm->data = data;
    }
    spinlock_release(&shm->lock); // was locked previously in the for loop

    // step 2
    {
        reschedule_for_wait_condition(wc_wait_for(shm, wc_ipcshm_is_attached_or_freed, NULL));
        spinlock_acquire(&shm->lock); // blocked until the server has finished setting up the connection
        if (shm->state == IPCSHM_FREE)
        {
            pr_warn("connection was closed before it was accepted");
            *read_buf = NULL;
            *write_buf = NULL;
            return NULL;
        }
        mos_debug(ipc, "resuming after connection was accepted");
    }
    // step 3
    {
        const vmblock_t block = shm_map_shared_block(shm->server_write_shm, VMAP_FORK_PRIVATE);
        *read_buf = (void *) block.vaddr;
    }
    spinlock_release(&shm->lock);
    return shm;
}

ipcshm_t *ipcshm_accept(ipcshm_server_t *server, void **read_buf, void **write_buf, void **data_out)
{
    // find a pending connection and attach to it
    ipcshm_t *shm = NULL;

    if (server->magic != IPCSHM_SERVER_MAGIC)
    {
        pr_warn("ipcshm_accept: server magic is invalid (0x%x)", server->magic);
        return NULL;
    }

    spinlock_acquire(&server->pending_lock);
    for (size_t i = 0; i < server->max_pending; i++)
    {
        spinlock_acquire(&server->pending[i]->lock);
        if (server->pending[i]->state == IPCSHM_PENDING)
        {
            shm = server->pending[i];
            shm->state = IPCSHM_ATTACHED;
            // replace the pending connection with a new one
            server->pending[i] = kzalloc(sizeof(ipcshm_t));
            break;
        }
        spinlock_release(&server->pending[i]->lock);
    }
    spinlock_release(&server->pending_lock);

    if (unlikely(!shm))
    {
        mos_debug(ipc, "waiting for a pending connection");
        reschedule_for_wait_condition(wc_wait_for(server, wc_ipcshm_pending_or_closed, NULL));
        mos_debug(ipc, "resuming after pending connection");

        shm = ipcshm_server_get_pending(server);
        if (!shm)
        {
            pr_info2("ipcshm_accept: server was closed");
            return NULL;
        }
        spinlock_acquire(&shm->lock);
    }

    pr_info("ipcshm_accept: accepted connection");

    // there are 3 steps for a client to connect to a server:
    //
    // 1. allocate a shm, and set server_write_buffer
    // 2. map the client's shm, and set client_write_buffer
    // 3. wake up the client

    // shm->lock is already held
    {
        // step 1
        shm->server_write_shm = shm_allocate(shm->buffer_size / MOS_PAGE_SIZE, VMAP_FORK_PRIVATE, VM_USER_RW);
        *write_buf = (void *) shm->server_write_shm.vaddr;

        // step 2
        const vmblock_t block = shm_map_shared_block(shm->client_write_shm, VMAP_FORK_PRIVATE);
        *read_buf = (void *) block.vaddr;

        // step 3
        shm->state = IPCSHM_ATTACHED;

        if (data_out)
            *data_out = shm->data;
    }
    spinlock_release(&shm->lock);

    return shm;
}

bool ipcshm_deannounce(ipcshm_server_t *server)
{
    spinlock_acquire(&billboard_lock);
    ipcshm_server_t *removed_server = hashmap_remove(ipcshm_billboard, (uintptr_t) server->name);
    spinlock_release(&billboard_lock);

    if (removed_server != server)
    {
        pr_warn("the given server instance was not found in the billboard");
        return false;
    }

    if (server->magic != IPCSHM_SERVER_MAGIC)
    {
        pr_warn("server magic is invalid (0x%x)", server->magic);
        return false;
    }

    ipcfs_unregister_server(server);

    for (size_t i = 0; i < server->max_pending; i++)
    {
        // free all pending connections
        ipcshm_t *shm = server->pending[i];
        spinlock_acquire(&shm->lock); // lock the connection
        kfree(shm);
    }

    kfree(server->name);
    kfree(server->pending);
    memzero(server, sizeof(ipcshm_server_t));
    kfree(server);
    return true;
}
