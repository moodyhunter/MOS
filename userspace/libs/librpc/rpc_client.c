// SPDX-License-Identifier: GPL-3.0-or-later

#include "librpc/rpc_client.h"

#include "lib/memory.h"
#include "lib/string.h"
#include "lib/sync/mutex.h"
#include "libipc/ipc.h"
#include "librpc/internal.h"
#include "librpc/rpc.h"
#include "mos/syscall/usermode.h"

#define RPC_CLIENT_SMH_SIZE MOS_PAGE_SIZE

typedef struct rpc_server_stub
{
    const char *server_name;
    fd_t fd;
    mutex_t mutex; // only one call at a time
    atomic_t callid;
} rpc_server_stub_t;

typedef struct rpc_call
{
    rpc_server_stub_t *server;
    rpc_request_t *request;
    size_t size;
    mutex_t mutex;
} rpc_call_t;

rpc_server_stub_t *rpc_client_create(const char *server_name)
{
    rpc_server_stub_t *client = malloc(sizeof(rpc_server_stub_t));
    memzero(client, sizeof(rpc_server_stub_t));
    client->fd = syscall_ipc_connect(server_name, RPC_CLIENT_SMH_SIZE);
    client->server_name = server_name;

    if (client->fd < 0)
    {
        free(client);
        return NULL;
    }

    return client;
}

void rpc_client_destroy(rpc_server_stub_t *server)
{
    mutex_acquire(&server->mutex);
    syscall_io_close(server->fd);
    free(server);
}

rpc_call_t *rpc_call_create(rpc_server_stub_t *server, u32 function_id)
{
    rpc_call_t *call = malloc(sizeof(rpc_call_t));
    memzero(call, sizeof(rpc_call_t));
    mutex_acquire(&call->mutex);

    call->request = malloc(sizeof(rpc_request_t));
    memzero(call->request, sizeof(rpc_request_t));
    call->request->magic = RPC_REQUEST_MAGIC;
    call->request->function_id = function_id;
    call->size = sizeof(rpc_request_t);
    call->server = server;

    mutex_release(&call->mutex);
    return call;
}

void rpc_call_destroy(rpc_call_t *call)
{
    mutex_acquire(&call->mutex);
    free(call->request);
    free(call);
}

void rpc_call_arg(rpc_call_t *call, const void *data, size_t size)
{
    mutex_acquire(&call->mutex);
    call->request = realloc(call->request, call->size + sizeof(rpc_arg_t) + size);
    call->request->args_count += 1;

    rpc_arg_t *arg = (rpc_arg_t *) &call->request->args_array[call->size - sizeof(rpc_request_t)];
    arg->size = size;
    arg->magic = RPC_ARG_MAGIC;
    memcpy(arg->data, data, size);

    call->size += sizeof(rpc_arg_t) + size;
    mutex_release(&call->mutex);
}

rpc_result_code_t rpc_call_exec(rpc_call_t *call, void **result_data, size_t *data_size)
{
    if (result_data && data_size)
    {
        *data_size = 0;
        *result_data = NULL;
    }

    mutex_acquire(&call->mutex);
    mutex_acquire(&call->server->mutex);
    call->request->call_id = ++call->server->callid;

    bool written = ipc_write_as_msg(call->server->fd, (char *) call->request, call->size);
    if (!written)
    {
        mutex_release(&call->server->mutex);
        mutex_release(&call->mutex);
        return RPC_RESULT_CLIENT_WRITE_FAILED;
    }

    ipc_msg_t *msg = ipc_read_msg(call->server->fd);
    if (!msg)
    {
        mutex_release(&call->server->mutex);
        mutex_release(&call->mutex);
        return RPC_RESULT_CLIENT_READ_FAILED;
    }

    if (msg->size < sizeof(rpc_response_t))
    {
        mutex_release(&call->server->mutex);
        mutex_release(&call->mutex);
        return RPC_RESULT_CLIENT_READ_FAILED;
    }

    rpc_response_t *response = (rpc_response_t *) msg->data;
    if (response->magic != RPC_RESPONSE_MAGIC)
    {
        mutex_release(&call->server->mutex);
        mutex_release(&call->mutex);
        return RPC_RESULT_CLIENT_READ_FAILED;
    }

    if (response->call_id != call->request->call_id)
    {
        mutex_release(&call->server->mutex);
        mutex_release(&call->mutex);
        return RPC_RESULT_CALLID_MISMATCH;
    }

    if (response->result_code != RPC_RESULT_OK)
    {
        mutex_release(&call->server->mutex);
        mutex_release(&call->mutex);
        return response->result_code;
    }

    if (msg->size < sizeof(rpc_response_t))
    {
        mutex_release(&call->server->mutex);
        mutex_release(&call->mutex);
        return RPC_RESULT_CLIENT_READ_FAILED;
    }

    if (result_data && data_size && response->data_size)
    {
        *data_size = response->data_size;
        *result_data = malloc(response->data_size);
        memcpy(*result_data, response->data, response->data_size);
    }

    rpc_result_code_t result = response->result_code;
    ipc_msg_destroy(msg);
    mutex_release(&call->server->mutex);
    mutex_release(&call->mutex);
    return result;
}
