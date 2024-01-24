// SPDX-License-Identifier: GPL-3.0-or-later
// RPC library common definitions

#pragma once

#include <librpc/macro_magic.h>

typedef enum
{
    RPC_ARGTYPE_FLOAT32, // float
    RPC_ARGTYPE_FLOAT64, //
    RPC_ARGTYPE_INT8,    // signed char (s8)
    RPC_ARGTYPE_INT16,   // short (s16)
    RPC_ARGTYPE_INT32,   // int (s32)
    RPC_ARGTYPE_INT64,   // long long (s64)
    RPC_ARGTYPE_UINT8,   // u8
    RPC_ARGTYPE_UINT16,  // u16
    RPC_ARGTYPE_UINT32,  // u32
    RPC_ARGTYPE_UINT64,  // u64
    RPC_ARGTYPE_STRING,  // (const) char *
    RPC_ARGTYPE_BUFFER,  // (const) void *, size_t
} rpc_argtype_t;

typedef enum
{
    RPC_RESULT_OK,
    RPC_RESULT_SERVER_INVALID_FUNCTION,
    RPC_RESULT_SERVER_INVALID_ARG_COUNT,
    RPC_RESULT_SERVER_INTERNAL_ERROR,
    RPC_RESULT_INVALID_ARGUMENT,
    RPC_RESULT_CLIENT_INVALID_ARGSPEC,
    RPC_RESULT_CLIENT_WRITE_FAILED,
    RPC_RESULT_CLIENT_READ_FAILED,
    RPC_RESULT_CALLID_MISMATCH,
} rpc_result_code_t;
