// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "librpc/rpc.h"

#include <mos/types.h>

#define RPC_REQUEST_MAGIC  MOS_FOURCC('R', 'P', 'C', '>')
#define RPC_RESPONSE_MAGIC MOS_FOURCC('R', 'P', 'C', '<')
#define RPC_ARG_MAGIC      MOS_FOURCC('R', 'P', 'C', 'A')

typedef struct
{
    u32 magic; // RPC_ARG_MAGIC
    u32 size;
    char data[];
} rpc_arg_t;

typedef struct
{
    u32 magic; // RPC_REQUEST_MAGIC
    id_t call_id;

    u32 function_id;
    u32 args_count;
    char args_array[]; // rpc_arg_t[]
} rpc_request_t;

typedef struct
{
    u32 magic; // RPC_RESPONSE_MAGIC
    id_t call_id;

    rpc_result_code_t result_code;
    size_t data_size;
    char data[];
} rpc_response_t;
