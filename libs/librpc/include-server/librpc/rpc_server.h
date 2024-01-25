// SPDX-License-Identifier: GPL-3.0-or-later
// RPC library server-side definitions

#pragma once

#include "librpc/internal.h"
#include "librpc/rpc.h"

#include <mos/types.h>

#define RPC_MAX_ARGS 8

typedef struct _rpc_server rpc_server_t;
typedef struct _rpc_context rpc_context_t;

typedef rpc_result_code_t (*rpc_function_t)(rpc_server_t *server, rpc_context_t *context, void *data);

typedef struct rpc_function_info
{
    u32 function_id;
    rpc_function_t func;
    u32 args_count;
    rpc_argtype_t args_type[RPC_MAX_ARGS];
} rpc_function_info_t;

/**
 * @brief Create a new RPC server
 *
 * @param server_name The name of the server
 * @param data A pointer to user data, which will be passed to the function
 * @return rpc_server_t* A pointer to the new server
 */
MOSAPI rpc_server_t *rpc_server_create(const char *server_name, void *data);

/**
 * @brief Set the user data for the server
 *
 * @param server The server to set the data for
 * @param data The data to set
 */
MOSAPI void rpc_server_set_data(rpc_server_t *server, void *data);

/**
 * @brief Get the user data for the server
 *
 * @param server The server to get the data for
 */
MOSAPI void *rpc_server_get_data(rpc_server_t *server);

/**
 * @brief Run the server, this function will not return until the server is destroyed
 *
 * @param server The server to run
 *
 * @note The incoming RPC calls will be handled in a separate thread.
 */
MOSAPI void rpc_server_exec(rpc_server_t *server);

/**
 * @brief Register multiple functions with the server
 *
 * @param server The server to register the functions with
 * @param functions An array of function info structures
 * @param count The number of functions to register
 * @return true The functions were registered successfully
 * @return false The functions could not be registered
 */
MOSAPI bool rpc_server_register_functions(rpc_server_t *server, const rpc_function_info_t *functions, size_t count);

/**
 * @brief Iterate to the next argument
 *
 * @param args The argument iterator
 * @param size A pointer to the size of the argument
 * @return const void* A pointer to the next argument
 *
 * @note Do not modify any of the data in the returned pointer.
 */
MOSAPI const void *rpc_arg_next(rpc_context_t *args, size_t *size);

/**
 * @brief Iterate to the next argument, and check that the size is as expected
 *
 * @param args The argument iterator
 * @param expected_size The expected size of the argument
 * @return const void* A pointer to the next argument, or NULL if the size is incorrect.
 *
 * @note Do not modify any of the data in the returned pointer.
 */
MOSAPI const void *rpc_arg_sized_next(rpc_context_t *iter, size_t expected_size);

MOSAPI const void *rpc_arg(const rpc_context_t *context, size_t iarg, rpc_argtype_t type, size_t *argsize);

MOSAPI u8 rpc_arg_u8(const rpc_context_t *context, size_t iarg);
MOSAPI u16 rpc_arg_u16(const rpc_context_t *context, size_t iarg);
MOSAPI u32 rpc_arg_u32(const rpc_context_t *context, size_t iarg);
MOSAPI u64 rpc_arg_u64(const rpc_context_t *context, size_t iarg);
MOSAPI s8 rpc_arg_s8(const rpc_context_t *context, size_t iarg);
MOSAPI s16 rpc_arg_s16(const rpc_context_t *context, size_t iarg);
MOSAPI s32 rpc_arg_s32(const rpc_context_t *context, size_t iarg);
MOSAPI s64 rpc_arg_s64(const rpc_context_t *context, size_t iarg);

// so we can use protobuf (nanopb) with librpc
#define rpc_arg_pb(type, val, context, argid)                                                                                                                            \
    statement_expr(bool, {                                                                                                                                               \
        size_t size = 0;                                                                                                                                                 \
        const void *payload = rpc_arg(context, argid, RPC_ARGTYPE_BUFFER, &size);                                                                                        \
        pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *) payload, size);                                                                                 \
        retval = pb_decode(&stream, type##_fields, &val);                                                                                                                \
    })

/**
 * @brief Write a result to the reply
 *
 * @param result The result object
 * @param data The data to write
 * @param size The size of the data to write
 */
MOSAPI void rpc_write_result(rpc_context_t *context, const void *data, size_t size);

#define rpc_write_result_pb(type, val, context)                                                                                                                          \
    statement_expr(bool, {                                                                                                                                               \
        uint8_t buffer[8192];                                                                                                                                            \
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));                                                                                            \
        retval = pb_encode(&stream, type##_fields, &val);                                                                                                                \
        if (retval)                                                                                                                                                      \
            rpc_write_result(context, buffer, stream.bytes_written);                                                                                                     \
    })

MOSAPI void rpc_server_close(rpc_server_t *server);

/**
 * @brief Destroy the RPC server
 *
 * @param server The server to destroy
 */
MOSAPI void rpc_server_destroy(rpc_server_t *server);
