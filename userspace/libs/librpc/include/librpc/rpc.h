// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "mos/types.h"

typedef enum
{
    RPC_ARGTYPE_INVALID = 0,
    RPC_ARGTYPE_INT,
    RPC_ARGTYPE_CHAR,
    RPC_ARGTYPE_STRING,
} rpc_argtype_t;

#define RPC_REQUEST_MAGIC MOS_FOURCC('R', 'P', 'C', '>')
#define RPC_REPLY_MAGIC   MOS_FOURCC('R', 'P', 'C', '<')
#define RPC_ARG_MAGIC     MOS_FOURCC('R', 'P', 'C', 'A')

typedef struct rpc_server rpc_server_t;
typedef struct rpc_request rpc_request_t;
typedef struct rpc_reply rpc_reply_t;

typedef struct
{
    u32 magic; // RPC_ARG_MAGIC
    rpc_argtype_t type;
    size_t content_size;
    char content[];
} rpc_arg_t;

typedef struct rpc_request
{
    u32 magic; // RPC_REQUEST_MAGIC
    id_t call_id;

    u32 function_id;
    u32 args_count;
    char args[];
} rpc_request_t;

typedef struct rpc_reply
{
    u32 magic; // RPC_REPLY_MAGIC
    id_t call_id;

    u32 status;
    u32 reply_size;
    char reply[];
} rpc_reply_t;

typedef int (*rpc_function_t)(rpc_server_t *server, rpc_request_t *request, rpc_reply_t **reply);

typedef struct rpc_function_info
{
    u32 function_id;
    rpc_function_t func;
    size_t args_count;
} rpc_function_info_t;

rpc_server_t *rpc_create_server(const char *server_name, void *data);
void rpc_destroy_server(rpc_server_t *server);
void rpc_server_run(rpc_server_t *server);

bool rpc_register_function(rpc_server_t *server, rpc_function_info_t function_info);
