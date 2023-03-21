// SPDX-License-Identifier: GPL-3.0-or-later
// RPC library common definitions

#pragma once

#include <mos/types.h>

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
