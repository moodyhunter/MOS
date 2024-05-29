// SPDX-License-Identifier: GPL-3.0-or-later

#include "librpc/rpc_server.h"

#include "librpc/internal.h"
#include "librpc/rpc.h"

#include <libipc/ipc.h>
#include <mos/types.h>

#if defined(__MOS_KERNEL__)
#include <mos/lib/sync/mutex.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>
#else
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MOS_LIB_ASSERT_X(cond, msg) assert(cond &&msg)
#define MOS_LIB_ASSERT(cond)        assert(cond)
#endif

#ifdef __MOS_KERNEL__
#include "mos/io/io.h"
#include "mos/ipc/ipc_io.h"
#include "mos/tasks/kthread.h"

#include <mos/syscall/decl.h>
#define syscall_ipc_create(server_name, max_pending) ipc_create(server_name, max_pending)
#define syscall_ipc_accept(server_fd)                io_ref(ipc_accept(server_fd))
#define syscall_ipc_connect(server_name, smh_size)   ipc_connect(server_name, smh_size)
#define start_thread(name, func, arg)                kthread_create(func, arg, name)
#define syscall_io_close(fd)                         io_unref(fd)
#else
#include <mos/syscall/usermode.h>
#endif

#if !defined(__MOS_KERNEL__)
// fixup for hosted libc
#include <pthread.h>
typedef pthread_mutex_t mutex_t;
#define memzero(ptr, size)    memset(ptr, 0, size)
#define mutex_acquire(mutex)  pthread_mutex_lock(mutex)
#define mutex_release(mutex)  pthread_mutex_unlock(mutex)
#define mos_warn(fmt, ...)    fprintf(stderr, fmt "\n", ##__VA_ARGS__)
#define MOS_LIB_UNREACHABLE() __builtin_unreachable()
static void start_thread(const char *name, thread_entry_t entry, void *arg)
{
    union
    {
        thread_entry_t entry;
        void *(*func)(void *);
    } u = { entry }; // to make the compiler happy
    pthread_t thread;
    pthread_create(&thread, NULL, u.func, arg);
    pthread_setname_np(thread, name);
}
#endif

#define RPC_SERVER_MAX_PENDING_CALLS 32

typedef struct _rpc_server
{
    const char *server_name;
    void *data;
    ipcfd_t server_fd;
    size_t functions_count;
    rpc_function_info_t *functions;
    rpc_server_on_connect_t on_connect;
    rpc_server_on_disconnect_t on_disconnect;
} rpc_server_t;

typedef struct _rpc_args_iter
{
    size_t next_arg_index;
    size_t next_arg_byte;
} rpc_args_iter_t;

struct _rpc_reply_wrapper
{
    rpc_response_t *response; // may be relocated by rpc_fill_result
};

struct _rpc_context
{
    ipcfd_t client_fd;
    rpc_server_t *server;
    rpc_request_t *request;
    rpc_response_t *response;
    rpc_args_iter_t arg_iter;
    void *data;
};

static inline rpc_function_info_t *rpc_server_get_function(rpc_server_t *server, u32 function_id)
{
    for (size_t i = 0; i < server->functions_count; i++)
        if (server->functions[i].function_id == function_id)
            return &server->functions[i];
    return NULL;
}

static void rpc_handle_client(void *arg)
{
    rpc_context_t *context = (rpc_context_t *) arg;

    if (context->server->on_connect)
        context->server->on_connect(context);

    while (true)
    {
        ipc_msg_t *const msg = ipc_read_msg(context->client_fd);
        if (!msg)
            break;

        if (msg->size < sizeof(rpc_request_t))
        {
            mos_warn("failed to read message from client");
            ipc_msg_destroy(msg);
            break;
        }

        rpc_request_t *request = (rpc_request_t *) msg->data;
        if (request->magic != RPC_REQUEST_MAGIC)
        {
            mos_warn("invalid magic in rpc request: %x", request->magic);
            ipc_msg_destroy(msg);
            break;
        }

        rpc_function_info_t *function = rpc_server_get_function(context->server, request->function_id);
        if (!function)
        {
            mos_warn("invalid function id in rpc request: %d", request->function_id);
            ipc_msg_destroy(msg);
            break;
        }

        if (request->args_count > RPC_MAX_ARGS)
        {
            mos_warn("too many arguments in rpc request: %d", request->args_count);
            ipc_msg_destroy(msg);
            break;
        }

        if (request->args_count != function->args_count)
        {
            mos_warn("invalid number if arguments in rpc request, expected %d, got %d", function->args_count, request->args_count);
            ipc_msg_destroy(msg);
            break;
        }

        // check argument types
        const char *argptr = request->args_array;
        for (size_t i = 0; i < request->args_count; i++)
        {
            const rpc_arg_t *arg = (const rpc_arg_t *) argptr;
            if (arg->magic != RPC_ARG_MAGIC)
            {
                mos_warn("invalid magic in rpc argument: %x", arg->magic);
                ipc_msg_destroy(msg);
                break;
            }
            if (arg->argtype != function->args_type[i])
            {
                mos_warn("invalid argument type in rpc request, expected %d, got %d", function->args_type[i], arg->argtype);
                ipc_msg_destroy(msg);
                break;
            }
            argptr += sizeof(rpc_arg_t) + arg->size;
        }
        context->request = request;
        context->response = NULL;
        context->arg_iter = (rpc_args_iter_t){ 0 };

        const rpc_result_code_t result = function->func(context);

        if (context->response == NULL)
        {
            context->response = malloc(sizeof(rpc_response_t));
            context->response->magic = RPC_RESPONSE_MAGIC;
            context->response->call_id = request->call_id;
            context->response->data_size = 0;
        }

        context->response->result_code = result;

        const bool written = ipc_write_as_msg(context->client_fd, (const char *) context->response, sizeof(rpc_response_t) + context->response->data_size);

        ipc_msg_destroy(msg);
        free(context->response);
        context->response = NULL, context->request = NULL, context->arg_iter = (rpc_args_iter_t){ 0 };

        if (!written)
        {
            mos_warn("failed to write reply to client");
            break;
        }
    }

    if (context->server->on_disconnect)
        context->server->on_disconnect(context);

    syscall_io_close(context->client_fd);
    free(context);
}

rpc_server_t *rpc_server_create(const char *server_name, void *data)
{
    rpc_server_t *server = malloc(sizeof(rpc_server_t));
    memzero(server, sizeof(rpc_server_t));
    server->server_name = server_name;
    server->data = data;
#ifndef __MOS_KERNEL__
    server->server_fd = -1;
#endif
    server->functions_count = 0;
    server->functions = NULL;
    server->server_fd = syscall_ipc_create(server_name, RPC_SERVER_MAX_PENDING_CALLS);
    if (IS_ERR_VALUE(server->server_fd))
    {
#if !defined(__MOS_KERNEL__)
        errno = -server->server_fd;
#endif
        free(server);
        return NULL;
    }
    return server;
}

void rpc_server_set_on_connect(rpc_server_t *server, rpc_server_on_connect_t on_connect)
{
    server->on_connect = on_connect;
}

void rpc_server_set_on_disconnect(rpc_server_t *server, rpc_server_on_disconnect_t on_disconnect)
{
    server->on_disconnect = on_disconnect;
}

void rpc_server_close(rpc_server_t *server)
{
    syscall_io_close(server->server_fd);
    server->server_fd = (ipcfd_t) -1;
}

void rpc_server_destroy(rpc_server_t *server)
{
    if (!IS_ERR_VALUE(server->server_fd))
        syscall_io_close(server->server_fd);
    if (server->functions)
        free(server->functions);
    free(server);
}

void rpc_server_set_data(rpc_server_t *server, void *data)
{
    server->data = data;
}

void *rpc_server_get_data(rpc_server_t *server)
{
    return server->data;
}

void rpc_server_exec(rpc_server_t *server)
{
    while (true)
    {
        const ipcfd_t client_fd = syscall_ipc_accept(server->server_fd);

        if (IS_ERR_VALUE(client_fd))
        {
            if ((long) client_fd == -EINTR)
                continue;

            if ((long) client_fd == -ECONNABORTED)
                break; // server closed

#if !defined(__MOS_KERNEL__)
            errno = -client_fd;
#endif
            break;
        }

        rpc_context_t *context = malloc(sizeof(rpc_context_t));
        context->server = server;
        context->client_fd = client_fd;
        start_thread("rpc-client-worker", rpc_handle_client, context);
    }
}

bool rpc_server_register_functions(rpc_server_t *server, const rpc_function_info_t *functions, size_t count)
{
    MOS_LIB_ASSERT_X(server->functions == NULL, "cannot register multiple times");
    server->functions = malloc(sizeof(rpc_function_info_t) * count);
    memcpy(server->functions, functions, sizeof(rpc_function_info_t) * count);
    server->functions_count = count;
    return true;
}

void *rpc_context_get_data(const rpc_context_t *context)
{
    return context->data;
}

void *rpc_context_set_data(rpc_context_t *context, void *data)
{
    void *old = NULL;
    __atomic_exchange(&context->data, &data, &old, __ATOMIC_SEQ_CST);
    return old;
}

rpc_server_t *rpc_context_get_server(const rpc_context_t *context)
{
    return context->server;
}

MOSAPI int rpc_context_get_function_id(const rpc_context_t *context)
{
    if (!context->request)
        return -1;
    return context->request->function_id;
}

const void *rpc_arg_next(rpc_context_t *context, size_t *size)
{
    if (context->arg_iter.next_arg_index >= context->request->args_count)
        return NULL;

    rpc_args_iter_t *const args = &context->arg_iter;

    const size_t next_arg_byte = args->next_arg_byte;

    const rpc_arg_t *arg = (rpc_arg_t *) &context->request->args_array[next_arg_byte];
    if (arg->magic != RPC_ARG_MAGIC)
        return NULL;

    args->next_arg_index++;
    args->next_arg_byte += sizeof(rpc_arg_t) + arg->size;

    if (size)
        *size = arg->size;

    return arg->data;
}

const void *rpc_arg_sized_next(rpc_context_t *context, size_t expected_size)
{
    size_t size = 0;
    const void *data = rpc_arg_next(context, &size);
    if (size != expected_size)
        return NULL;
    return (void *) data;
}

#define RPC_ARG_NEXT_IMPL(type, TYPE)                                                                                                                                    \
    type rpc_arg_next_##type(rpc_context_t *context)                                                                                                                     \
    {                                                                                                                                                                    \
        return *(type *) rpc_arg_next(context, NULL);                                                                                                                    \
    }

RPC_ARG_NEXT_IMPL(u8, UINT8)
RPC_ARG_NEXT_IMPL(u16, UINT16)
RPC_ARG_NEXT_IMPL(u32, UINT32)
RPC_ARG_NEXT_IMPL(u64, UINT64)
RPC_ARG_NEXT_IMPL(s8, INT8)
RPC_ARG_NEXT_IMPL(s16, INT16)
RPC_ARG_NEXT_IMPL(s32, INT32)
RPC_ARG_NEXT_IMPL(s64, INT64)

const char *rpc_arg_next_string(rpc_context_t *context)
{
    return rpc_arg_next(context, NULL);
}

const void *rpc_arg(const rpc_context_t *context, size_t iarg, rpc_argtype_t type, size_t *argsize)
{
    // iterate over arguments
    const char *ptr = context->request->args_array;
    for (size_t i = 0; i < iarg; i++)
    {
        const rpc_arg_t *arg = (const rpc_arg_t *) ptr;
        MOS_LIB_ASSERT(arg->magic == RPC_ARG_MAGIC);
        ptr += sizeof(rpc_arg_t) + arg->size;
    }

    const rpc_arg_t *arg = (const rpc_arg_t *) ptr;
    MOS_LIB_ASSERT(arg->magic == RPC_ARG_MAGIC);
    MOS_LIB_ASSERT(arg->argtype == type);
    if (argsize)
        *argsize = arg->size;
    return arg->data;
}

#define RPC_GET_ARG_IMPL(type, TYPE)                                                                                                                                     \
    type rpc_arg_##type(const rpc_context_t *context, size_t iarg)                                                                                                       \
    {                                                                                                                                                                    \
        return *(type *) rpc_arg(context, iarg, RPC_ARGTYPE_##TYPE, NULL);                                                                                               \
    }

RPC_GET_ARG_IMPL(u8, UINT32)
RPC_GET_ARG_IMPL(u16, UINT32)
RPC_GET_ARG_IMPL(u32, UINT32)
RPC_GET_ARG_IMPL(u64, UINT64)
RPC_GET_ARG_IMPL(s8, INT32)
RPC_GET_ARG_IMPL(s16, INT32)
RPC_GET_ARG_IMPL(s32, INT32)
RPC_GET_ARG_IMPL(s64, INT64)

const char *rpc_arg_string(const rpc_context_t *context, size_t iarg)
{
    return (const char *) rpc_arg(context, iarg, RPC_ARGTYPE_STRING, NULL);
}

void rpc_write_result(rpc_context_t *context, const void *data, size_t size)
{
    MOS_LIB_ASSERT_X(context->response == NULL, "rpc_write_result called twice");

    rpc_response_t *response = malloc(sizeof(rpc_response_t) + size);
    response->magic = RPC_RESPONSE_MAGIC;
    response->call_id = context->request->call_id;
    response->result_code = RPC_RESULT_OK;
    response->data_size = size;
    memcpy(response->data, data, size);
    context->response = response;
}
