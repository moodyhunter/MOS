// SPDX-License-Identifier: GPL-3.0-or-later
// RPC library client-side definitions

#pragma once

#include "librpc/rpc.h"
#include "mos/types.h"

typedef struct rpc_server_stub rpc_server_stub_t;
typedef struct rpc_call rpc_call_t;

rpc_server_stub_t *rpc_client_create(const char *server_name);
void rpc_client_destroy(rpc_server_stub_t *server);

rpc_call_t *rpc_call_create(rpc_server_stub_t *server, u32 function_id);
void rpc_call_destroy(rpc_call_t *call);
void rpc_call_arg(rpc_call_t *call, const void *data, size_t size);
rpc_result_code_t rpc_call_exec(rpc_call_t *call, void **result_data, size_t *result_size);
