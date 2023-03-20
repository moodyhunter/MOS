// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "lib/sync/mutex.h"
#include "mos/mm/mm_types.h"
#include "mos/mm/shm.h"
#include "mos/tasks/task_types.h"

typedef enum
{
    IPCSHM_FREE,
    IPCSHM_PENDING,
    IPCSHM_ATTACHED,
} ipcshm_state;

typedef struct ipcshm_t ipcshm_t;
typedef struct ipcshm_server_t
{
    u32 magic;
    const char *name;
    size_t max_pending;
    spinlock_t pending_lock;
    ipcshm_t **pending;
} ipcshm_server_t;

typedef struct ipcshm_t
{
    ipcshm_server_t *server;
    size_t buffer_size;
    ipcshm_state state;
    spinlock_t lock;
    vmblock_t client_write_shm;
    vmblock_t server_write_shm;
    void *data;
} ipcshm_t;

void ipcshm_init(void);

/**
 * @brief Announce a new IPC SHM server
 * @param name The name of the server
 * @param max_pending The maximum number of pending connections to allow for this server
 * @return A new io_t object that represents the server, or NULL on failure
 */
ipcshm_server_t *ipcshm_announce(const char *name, size_t max_pending);

/**
 * @brief Connect to an IPC SHM server
 * @param name The name announced by the server
 * @param buffer_size The size of the shared memory buffer to use for the connection
 * @param server_write_buffer A pointer to a void pointer that will be filled with the server's write buffer
 * @param client_write_buffer A pointer to a void pointer that will be filled with the client's write buffer
 * @return A new ipcshm_t object that represents the connection, or NULL on failure
 */
ipcshm_t *ipcshm_request(const char *name, size_t buffer_size, void **server_write_buffer, void **client_write_buffer, void *data);

/**
 * @brief Accept a new connection on an IPC SHM server
 * @param server The server to accept a connection on
 * @param server_write_buffer A pointer to a ipcshm_buffer_t pointer that will be filled with the server's write buffer
 * @param client_write_buffer A pointer to a ipcshm_buffer_t pointer that will be filled with the client's write buffer
 * @return A new ipcshm_t object that represents the connection, or NULL on failure
 */
ipcshm_t *ipcshm_accept(ipcshm_server_t *server, void **server_write_buffer, void **client_write_buffer, void **data_out);

/**
 * @brief Deannounce an IPC SHM server
 * @param server The server name to deannounce
 * @return true on success, false on failure
 */
bool ipcshm_deannounce(ipcshm_server_t *server);
