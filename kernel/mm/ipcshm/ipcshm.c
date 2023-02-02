// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/mm/ipcshm/ipcshm.h"

#include "lib/stdlib.h"
#include "lib/string.h"
#include "lib/structures/hashmap.h"
#include "lib/structures/hashmap_common.h"
#include "lib/sync/spinlock.h"
#include "mos/mm/kmalloc.h"
#include "mos/mm/shm.h"
#include "mos/printk.h"
#include "mos/tasks/schedule.h"
#include "mos/tasks/wait.h"

#define IPCSHM_BILLBOARD_HASHMAP_SIZE 64
#define IPCSHM_SERVER_MAGIC           MOS_FOURCC('I', 'S', 'H', 'M')

static hashmap_t *ipcshm_billboard;
static spinlock_t billboard_lock;

static hash_t ipcshm_server_hash(const void *key)
{
    return hashmap_hash_string(key);
}

static int ipcshm_server_compare(const void *a, const void *b)
{
    return strcmp((const char *) a, (const char *) b) == 0;
}

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

static bool wc_ipcshm_has_pending(wait_condition_t *cond)
{
    ipcshm_server_t *server = cond->arg;
    return ipcshm_server_get_pending(server) != NULL;
}

static bool wc_ipcshm_is_attached_or_freed(wait_condition_t *cond)
{
    ipcshm_t *conn = cond->arg;
    return conn->state == IPCSHM_ATTACHED || conn->state == IPCSHM_FREE;
}

static bool wc_ipcshm_server_exists(wait_condition_t *cond)
{
    const char *name = cond->arg;
    spinlock_acquire(&billboard_lock);
    ipcshm_server_t *server = hashmap_get(ipcshm_billboard, name);
    spinlock_release(&billboard_lock);
    return server != NULL;
}

static void wc_ipcshm_server_free(wait_condition_t *cond)
{
    const char *name = cond->arg;
    kfree(name);
}

void ipcshm_init(void)
{
    pr_info("initializing shared-memory IPC backend");
    ipcshm_billboard = kzalloc(sizeof(hashmap_t));
    hashmap_init(ipcshm_billboard, IPCSHM_BILLBOARD_HASHMAP_SIZE, ipcshm_server_hash, ipcshm_server_compare);
}

ipcshm_server_t *ipcshm_announce(const char *name, size_t max_pending)
{
    spinlock_acquire(&billboard_lock);
    ipcshm_server_t *server = hashmap_get(ipcshm_billboard, name);
    spinlock_release(&billboard_lock);

    if (unlikely(server))
    {
        pr_warn("IPC channel '%s' already exists", name);
        return NULL;
    }

    pr_info("ipc: channel '%s' created", name);
    server = kzalloc(sizeof(ipcshm_server_t));
    server->magic = IPCSHM_SERVER_MAGIC;
    server->name = strdup(name);
    server->max_pending = max_pending;
    server->pending = kzalloc(sizeof(ipcshm_t *) * max_pending);
    for (size_t i = 0; i < max_pending; i++)
        server->pending[i] = kzalloc(sizeof(ipcshm_t));

    spinlock_acquire(&billboard_lock);
    hashmap_put(ipcshm_billboard, server->name, server);
    spinlock_release(&billboard_lock);
    return server;
}

bool ipcshm_request(const char *name, size_t buffer_size, void **read_buf, void **write_buf, void *data)
{
    pr_info("ipc: connecting to channel '%s'", name);
    buffer_size = ALIGN_UP(buffer_size, MOS_PAGE_SIZE);

    spinlock_acquire(&billboard_lock);
    ipcshm_server_t *server = hashmap_get(ipcshm_billboard, name);
    spinlock_release(&billboard_lock);

    if (unlikely(!server))
    {
        mos_debug(ipc, "no server found for channel '%s', waiting...", name);
        reschedule_for_wait_condition(wc_wait_for(strdup(name), wc_ipcshm_server_exists, NULL));
        mos_debug(ipc, "server for channel '%s' found, connecting...", name);

        spinlock_acquire(&billboard_lock);
        server = hashmap_get(ipcshm_billboard, name);
        spinlock_release(&billboard_lock);

        if (unlikely(!server))
        {
            pr_warn("server for channel '%s' disappeared after it was found", name);
            return false;
        }

        mos_debug(ipc, "server for channel '%s' found", name);
    }

    mos_debug(ipc, "connecting to channel '%s'", name);

    ipcshm_t *shm = NULL;

    if (server->magic != IPCSHM_SERVER_MAGIC)
    {
        pr_warn("ipcshm_request: server magic is invalid (0x%x)", server->magic);
        return false;
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
            // keep the lock held intentionally
            break;
        }
        spinlock_release(&server->pending[i]->lock);
    }
    spinlock_release(&server->pending_lock);

    if (unlikely(!shm))
    {
        pr_warn("ipcshm_request: no pending connection slots available");
        *read_buf = NULL;
        *write_buf = NULL;
        return false;
    }

    // there are 3 steps for a client to connect to a server:
    //
    // 1. client: allocates shm, and sets client_write_buffer
    // 2. client: waits for the server to accept the connection and set server_write_buffer
    // 3. client is woken up, and sets server_write_buffer

    // step 1
    {
        shm->client_write_shm = shm_allocate(shm->buffer_size / MOS_PAGE_SIZE, MMAP_PRIVATE, VM_USER_RW);
        *write_buf = (void *) shm->client_write_shm.block.vaddr;
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
            return false;
        }
        mos_debug(ipc, "resuming after connection was accepted");
    }
    // step 3
    {
        const vmblock_t block = shm_map_shared_block(shm->server_write_shm);
        *read_buf = (void *) block.vaddr;
    }
    spinlock_release(&shm->lock);
    return true;
}

bool ipcshm_accept(ipcshm_server_t *server, void **read_buf, void **write_buf, void **data_out)
{
    pr_info("ipc: accepting connection on channel '%s'", server->name);
    // find a pending connection and attach to it
    ipcshm_t *shm = NULL;

    if (server->magic != IPCSHM_SERVER_MAGIC)
    {
        pr_warn("ipcshm_accept: server magic is invalid (0x%x)", server->magic);
        return false;
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
        reschedule_for_wait_condition(wc_wait_for(server, wc_ipcshm_has_pending, wc_ipcshm_server_free));
        mos_debug(ipc, "resuming after pending connection");

        // TODO: check if the server was closed
        shm = ipcshm_server_get_pending(server);
        if (!shm)
        {
            pr_warn("ipcshm_accept: server was closed");
            return false;
        }
        spinlock_acquire(&shm->lock);
    }

    // there are 3 steps for a client to connect to a server:
    //
    // 1. allocate a shm, and set server_write_buffer
    // 2. map the client's shm, and set client_write_buffer
    // 3. wake up the client

    // shm->lock is already held
    {
        // step 1
        shm->server_write_shm = shm_allocate(shm->buffer_size / MOS_PAGE_SIZE, MMAP_PRIVATE, VM_USER_RW);
        *write_buf = (void *) shm->server_write_shm.block.vaddr;

        // step 2
        const vmblock_t block = shm_map_shared_block(shm->client_write_shm);
        *read_buf = (void *) block.vaddr;

        // step 3
        shm->state = IPCSHM_ATTACHED;

        if (data_out)
            *data_out = shm->data;
    }
    spinlock_release(&shm->lock);

    return true;
}

bool ipcshm_deannounce(const char *name)
{
    spinlock_acquire(&billboard_lock);
    ipcshm_server_t *server = hashmap_remove(ipcshm_billboard, name);
    spinlock_release(&billboard_lock);

    if (!server)
    {
        pr_warn("server %s was not announced", name);
        spinlock_release(&billboard_lock);
        return false;
    }

    if (server->magic != IPCSHM_SERVER_MAGIC)
    {
        pr_warn("ipcshm_deannounce: server magic is invalid (0x%x)", server->magic);
        return false;
    }

    for (size_t i = 0; i < server->max_pending; i++)
    {
        // free all pending connections
        kfree(server->pending[i]);
    }

    kfree(server->name);
    kfree(server->pending);
    kfree(server);
    return true;
}
