// SPDX-License-Identifier: GPL-3.0-or-later

#include "librpc/rpc_client.h"

#include "librpc/internal.h"
#include "librpc/rpc.h"

#include <libipc/ipc.h>
#include <stdarg.h>

#ifdef __MOS_KERNEL__
#include <mos/lib/sync/mutex.h>
#include <mos/syscall/decl.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>
#define syscall_ipc_connect(server_name, smh_size) impl_syscall_ipc_connect(server_name, smh_size)
#define syscall_io_close(fd)                       impl_syscall_io_close(fd)
#elifdef __MOS_MINIMAL_LIBC__
#include "mos/lib/sync/mutex.h"
#include "mos/moslib_global.h"

#include <mos/syscall/usermode.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>
#else
#include <mos/syscall/usermode.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef pthread_mutex_t mutex_t;
#define memzero(ptr, size)    memset(ptr, 0, size)
#define mutex_acquire(mutex)  pthread_mutex_lock(mutex)
#define mutex_release(mutex)  pthread_mutex_unlock(mutex)
#define mos_warn(...)         fprintf(stderr, __VA_ARGS__)
#define MOS_LIB_UNREACHABLE() __builtin_unreachable()
#endif

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
    client->server_name = server_name;
    client->fd = syscall_ipc_connect(server_name, RPC_CLIENT_SMH_SIZE);

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

    call->request = malloc(sizeof(rpc_request_t));
    memzero(call->request, sizeof(rpc_request_t));
    call->request->magic = RPC_REQUEST_MAGIC;
    call->request->function_id = function_id;
    call->size = sizeof(rpc_request_t);
    call->server = server;

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

rpc_result_code_t rpc_simple_call(rpc_server_stub_t *stub, u32 funcid, rpc_result_t *result, const char *argspec, ...)
{
    va_list args;
    va_start(args, argspec);
    rpc_result_code_t code = rpc_simple_callv(stub, funcid, result, argspec, args);
    va_end(args);
    return code;
}

rpc_result_code_t rpc_simple_callv(rpc_server_stub_t *stub, u32 funcid, rpc_result_t *result, const char *argspec, va_list args)
{
    if (unlikely(!argspec))
    {
        mos_warn("argspec is NULL");
        return RPC_RESULT_CLIENT_INVALID_ARGSPEC;
    }

    rpc_call_t *call = rpc_call_create(stub, funcid);

    if (*argspec == 'v')
    {
        if (*++argspec != '\0')
        {
            mos_warn("argspec is not empty after 'v' (void) (argspec='%s')", argspec);
            rpc_call_destroy(call);
            return RPC_RESULT_CLIENT_INVALID_ARGSPEC;
        }
        goto exec;
    }

    for (const char *c = argspec; *c != '\0'; c++)
    {
        switch (*c)
        {
            case 'c':
            {
                u8 arg = va_arg(args, int);
                rpc_call_arg(call, &arg, sizeof(arg));
                break;
            }
            case 'i':
            {
                u32 arg = va_arg(args, int);
                rpc_call_arg(call, &arg, sizeof(arg));
                break;
            }
            case 'l':
            {
                u64 arg = va_arg(args, long long);
                rpc_call_arg(call, &arg, sizeof(arg));
                break;
            }
            case 'f':
            {
                MOS_LIB_UNREACHABLE(); // TODO: implement
                // double arg = va_arg(args, double);
                // rpc_call_arg(call, &arg, sizeof(arg));
                break;
            }
            case 's':
            {
                const char *arg = va_arg(args, const char *);
                rpc_call_arg(call, arg, strlen(arg) + 1); // also send the null terminator
                break;
            }
            default: mos_warn("rpc_call: invalid argspec '%c'", *c); return RPC_RESULT_CLIENT_INVALID_ARGSPEC;
        }
    }

exec:
    rpc_call_exec(call, &result->data, &result->size);
    rpc_call_destroy(call);

    return RPC_RESULT_OK;
}
