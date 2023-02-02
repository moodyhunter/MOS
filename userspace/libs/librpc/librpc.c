// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/memory.h"
#include "librpc/rpc.h"
#include "libuserspace.h"
#include "mos/syscall/usermode.h"
#include "mos/types.h"

#define RPC_SERVER_MAX_PENDING_CALLS 32

typedef struct rpc_server
{
    const char *server_name;
    void *data;
    fd_t server_fd;
    size_t functions_count;
    rpc_function_info_t *functions;
} rpc_server_t;

typedef struct rpc_call_context
{
    rpc_server_t *server;
    fd_t client_fd;
} rpc_call_context_t;

static void rpc_invoke_call(void *arg)
{
    rpc_call_context_t *context = (rpc_call_context_t *) arg;
    rpc_server_t *server = context->server;
    fd_t client_fd = context->client_fd;
    free(context);

    syscall_io_close(client_fd);
}

rpc_server_t *rpc_create_server(const char *server_name, void *data)
{
    rpc_server_t *server = malloc(sizeof(rpc_server_t));
    server->server_name = server_name;
    server->data = data;
    server->server_fd = -1;
    server->functions_count = 0;
    server->functions = NULL;
    server->server_fd = syscall_ipc_create(server_name, RPC_SERVER_MAX_PENDING_CALLS);
    return server;
}

void rpc_destroy_server(rpc_server_t *server)
{
    if (server)
    {
        if (server->server_fd != -1)
            syscall_io_close(server->server_fd);
        if (server->functions)
            free(server->functions);
        free(server);
    }
}

void rpc_server_run(rpc_server_t *server)
{
    while (true)
    {
        fd_t client_fd = syscall_ipc_accept(server->server_fd);
        if (client_fd == -1)
            continue;

        rpc_call_context_t *context = malloc(sizeof(rpc_call_context_t));
        context->server = server;
        context->client_fd = client_fd;
        start_thread("rpc-call", rpc_invoke_call, context);
    }
}

bool rpc_register_function(rpc_server_t *server, rpc_function_info_t function_info)
{
}
