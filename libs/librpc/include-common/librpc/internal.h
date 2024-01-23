// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <librpc/rpc.h>
#include <mos/types.h>

#define RPC_REQUEST_MAGIC  MOS_FOURCC('R', 'P', 'C', '>')
#define RPC_RESPONSE_MAGIC MOS_FOURCC('R', 'P', 'C', '<')
#define RPC_ARG_MAGIC      MOS_FOURCC('R', 'P', 'C', 'A')

typedef enum
{
    RPC_ARG_FLOAT32,
    RPC_ARG_INT8,
    RPC_ARG_FLOAT64,
    RPC_ARG_INT16,
    RPC_ARG_INT32,
    RPC_ARG_INT64,
    RPC_ARG_UINT8,
    RPC_ARG_UINT16,
    RPC_ARG_UINT32,
    RPC_ARG_UINT64,
    RPC_ARG_STRING,
    RPC_ARG_BUFFER,
} rpc_argtype_t;

typedef struct
{
    u32 magic; // RPC_ARG_MAGIC
    rpc_argtype_t argtype;
    u32 size;
    char data[];
} __packed rpc_arg_t;

typedef struct
{
    u32 magic; // RPC_REQUEST_MAGIC
    id_t call_id;

    u32 function_id;
    u32 args_count;
    char args_array[]; // rpc_arg_t[]
} __packed rpc_request_t;

typedef struct
{
    u32 magic; // RPC_RESPONSE_MAGIC
    id_t call_id;

    rpc_result_code_t result_code;
    size_t data_size;
    char data[];
} __packed rpc_response_t;
