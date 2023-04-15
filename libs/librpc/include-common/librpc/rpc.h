// SPDX-License-Identifier: GPL-3.0-or-later
// RPC library common definitions

#pragma once

typedef enum
{
    RPC_RESULT_OK,
    RPC_RESULT_SERVER_INVALID_FUNCTION,
    RPC_RESULT_SERVER_INVALID_ARG_COUNT,
    RPC_RESULT_SERVER_INVALID_ARG,
    RPC_RESULT_CLIENT_INVALID_ARGSPEC,
    RPC_RESULT_CLIENT_WRITE_FAILED,
    RPC_RESULT_CLIENT_READ_FAILED,
    RPC_RESULT_CALLID_MISMATCH,
} rpc_result_code_t;

#define DECLARE_FUNCTION_ID(_fid, _id, ...) _fid = _id,
#define DECLARE_FUNCTION_ID_ENUM(name, X_MACRO)                                                                                                                          \
    typedef enum                                                                                                                                                         \
    {                                                                                                                                                                    \
        X_MACRO(DECLARE_FUNCTION_ID)                                                                                                                                     \
    } name##_function_id_t;
