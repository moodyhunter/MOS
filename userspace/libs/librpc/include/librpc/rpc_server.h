// SPDX-License-Identifier: GPL-3.0-or-later
// RPC library server-side definitions

#pragma once

#include "librpc/rpc.h"
#include "mos/types.h"

typedef struct rpc_server rpc_server_t;
typedef struct rpc_args_iter rpc_args_iter_t;
typedef struct rpc_result rpc_result_t;

typedef int (*rpc_function_t)(rpc_server_t *server, rpc_args_iter_t *args, rpc_result_t *reply, void *data);

typedef struct rpc_function_info
{
    u32 function_id;
    rpc_function_t func;
    size_t args_count;
} rpc_function_info_t;

rpc_server_t *rpc_server_create(const char *server_name, void *data);
void rpc_server_destroy(rpc_server_t *server);
void rpc_server_exec(rpc_server_t *server);
bool rpc_server_register_function(rpc_server_t *server, u32 function_id, rpc_function_t func, size_t args_count);
bool rpc_server_register_functions(rpc_server_t *server, const rpc_function_info_t *functions, size_t count);

const void *rpc_arg_next(rpc_args_iter_t *args, size_t *size);
void rpc_write_result(rpc_result_t *result, const void *data, size_t size);
