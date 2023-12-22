// SPDX-License-Identifier: GPL-3.0-or-later

#include "librpc/rpc_server.h"

#include "librpc/internal.h"
#include "librpc/rpc.h"

#include <libipc/ipc.h>
#include <mos/types.h>

#if defined(__MOS_KERNEL__) || defined(__MOS_MINIMAL_LIBC__)
#include <mos/lib/sync/mutex.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>
#else
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#ifdef __MOS_KERNEL__
#include "mos/tasks/kthread.h"

#include <mos/syscall/decl.h>
#define syscall_ipc_create(server_name, max_pending_calls) impl_syscall_ipc_create(server_name, max_pending_calls)
#define syscall_ipc_accept(server_fd)                      impl_syscall_ipc_accept(server_fd)
#define syscall_ipc_connect(server_name, smh_size)         impl_syscall_ipc_connect(server_name, smh_size)
#define start_thread(name, func, arg)                      kthread_create(func, arg, name)
#define syscall_io_close(fd)                               impl_syscall_io_close(fd)
#else
#include <mos/syscall/usermode.h>
#endif

#if !defined(__MOS_KERNEL__) && !defined(__MOS_MINIMAL_LIBC__)
// fixup for hosted libc
#include <pthread.h>
typedef pthread_mutex_t mutex_t;
#define memzero(ptr, size)    memset(ptr, 0, size)
#define mutex_acquire(mutex)  pthread_mutex_lock(mutex)
#define mutex_release(mutex)  pthread_mutex_unlock(mutex)
#define mos_warn(...)         fprintf(stderr, __VA_ARGS__)
#define MOS_LIB_UNREACHABLE() __builtin_unreachable()

typedef struct
{
    thread_entry_t entry;
    void *arg;
} thread_start_args_t;

static void thread_start(void *_arg)
{
    thread_entry_t entry = ((thread_start_args_t *) _arg)->entry;
    void *entry_arg = ((thread_start_args_t *) _arg)->arg;
    entry(entry_arg);
    free(_arg);
    syscall_thread_exit();
}

static tid_t start_thread(const char *name, thread_entry_t entry, void *arg)
{
    thread_start_args_t *thread_start_args = malloc(sizeof(thread_start_args_t));
    thread_start_args->entry = entry;
    thread_start_args->arg = arg;
    return syscall_create_thread(name, thread_start, thread_start_args);
}
#endif

#define RPC_SERVER_MAX_PENDING_CALLS 32

typedef struct _rpc_server
{
    const char *server_name;
    void *data;
    fd_t server_fd;
    size_t functions_count;
    rpc_function_info_t *functions;
} rpc_server_t;

typedef struct _rpc_args_iter
{
    rpc_request_t *request;
    size_t next_arg_index;
    size_t next_arg_byte;
} rpc_args_iter_t;

struct _rpc_reply_wrapper
{
    rpc_response_t *response; // may be relocated by rpc_fill_result
};

typedef struct rpc_call_context
{
    rpc_server_t *server;
    fd_t client_fd;
} rpc_call_context_t;

static inline rpc_function_info_t *rpc_server_get_function(rpc_server_t *server, u32 function_id)
{
    for (size_t i = 0; i < server->functions_count; i++)
        if (server->functions[i].function_id == function_id)
            return &server->functions[i];
    return NULL;
}

static void rpc_handle_call(void *arg)
{
    rpc_call_context_t *context = (rpc_call_context_t *) arg;
    rpc_server_t *server = context->server;
    fd_t client_fd = context->client_fd;
    free(context);

    while (true)
    {
        ipc_msg_t *const msg = ipc_read_msg(client_fd);
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

        rpc_function_info_t *function = rpc_server_get_function(server, request->function_id);
        if (!function)
        {
            mos_warn("invalid function id in rpc request: %d", request->function_id);
            ipc_msg_destroy(msg);
            break;
        }

        if (request->args_count != function->args_count)
        {
            mos_warn("invalid number if arguments in rpc request, expected %d, got %d", function->args_count, request->args_count);
            ipc_msg_destroy(msg);
            break;
        }

        rpc_args_iter_t args = { request, 0, 0 };

        rpc_reply_t reply = { 0 };
        reply.response = malloc(sizeof(rpc_response_t));
        memzero(reply.response, sizeof(rpc_response_t));
        reply.response->magic = RPC_RESPONSE_MAGIC;
        reply.response->call_id = request->call_id;
        reply.response->result_code = function->func(server, &args, &reply, server->data);

        bool written = ipc_write_as_msg(client_fd, (const char *) reply.response, sizeof(rpc_response_t) + reply.response->data_size);

        ipc_msg_destroy(msg);
        free(reply.response);

        if (!written)
        {
            mos_warn("failed to write reply to client\n");
            break;
        }
    }

    syscall_io_close(client_fd);
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
    return server;
}

void rpc_server_destroy(rpc_server_t *server)
{
    if (server->server_fd != -1)
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
        fd_t client_fd = syscall_ipc_accept(server->server_fd);
        if (client_fd == -ECONNABORTED)
            break; // server closed

        if (client_fd < 0)
        {
            mos_warn("failed to accept client");
            break;
        }

        rpc_call_context_t *context = malloc(sizeof(rpc_call_context_t));
        context->server = server;
        context->client_fd = client_fd;
        start_thread("rpc-call", rpc_handle_call, context);
    }
}

bool rpc_server_register_functions(rpc_server_t *server, const rpc_function_info_t *functions, size_t count)
{
    for (size_t i = 0; i < count; i++)
        if (!rpc_server_register_function(server, functions[i].function_id, functions[i].func, functions[i].args_count))
            return false;
    return true;
}

bool rpc_server_register_function(rpc_server_t *server, u32 function_id, rpc_function_t func, size_t args_count)
{
    if (rpc_server_get_function(server, function_id))
        return false; // function already registered

    server->functions = realloc(server->functions, sizeof(rpc_function_info_t) * (server->functions_count + 1));
    server->functions[server->functions_count] = (rpc_function_info_t){ function_id, func, args_count };
    server->functions_count++;
    return true;
}

const void *rpc_arg_next(rpc_args_iter_t *args, size_t *size)
{
    if (args->next_arg_index >= args->request->args_count)
        return NULL;

    const size_t next_arg_byte = args->next_arg_byte;

    const rpc_arg_t *arg = (rpc_arg_t *) &args->request->args_array[next_arg_byte];
    if (arg->magic != RPC_ARG_MAGIC)
        return NULL;

    args->next_arg_index++;
    args->next_arg_byte += sizeof(rpc_arg_t) + arg->size;

    if (size)
        *size = arg->size;

    return arg->data;
}

const void *rpc_arg_sized_next(rpc_args_iter_t *iter, size_t expected_size)
{
    size_t size = 0;
    const void *data = rpc_arg_next(iter, &size);
    if (size != expected_size)
        return NULL;
    return (void *) data;
}

void rpc_write_result(rpc_reply_t *result, const void *data, size_t size)
{
    result->response->data_size = size;
    result->response = realloc(result->response, sizeof(rpc_response_t) + size);
    memcpy(result->response->data, data, size);
}
