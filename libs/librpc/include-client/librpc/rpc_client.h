// SPDX-License-Identifier: GPL-3.0-or-later
// RPC library client-side definitions

#pragma once

#include <librpc/rpc.h>
#include <mos/types.h>
#include <stdarg.h>

typedef struct rpc_server_stub rpc_server_stub_t;
typedef struct rpc_call rpc_call_t;

typedef struct
{
    void *data;
    size_t size;
} rpc_result_t;

/**
 * @brief Create a new RPC client stub for the given server
 *
 * @param server_name The name of the server to connect to
 * @return rpc_server_stub_t* A pointer to the new server stub, or NULL on error
 */
MOSAPI rpc_server_stub_t *rpc_client_create(const char *server_name);

/**
 * @brief Destroy a server stub
 *
 * @param server The server stub to destroy
 */
MOSAPI void rpc_client_destroy(rpc_server_stub_t *server);

/**
 * @brief Call a function on the server
 *
 * @param stub The server stub to call
 * @param funcid The function ID to call
 * @param result A pointer to a result structure, or NULL if no result is expected
 * @param argspec An argument specification string, see spec.md for details
 * @param ... The arguments to the function
 * @return rpc_result_code_t The result code of the call
 *
 * @note The result data will be malloc'd and must be freed by the caller.
 *
 * @note This function is a simple wrapper around rpc_call_create, rpc_call_arg, rpc_call_exec and rpc_call_destroy.
 */
MOSAPI rpc_result_code_t rpc_simple_call(rpc_server_stub_t *stub, u32 funcid, rpc_result_t *result, const char *argspec, ...);

/**
 * @brief Call a function on the server
 *
 * @param stub The server stub to call
 * @param funcid The function ID to call
 * @param result A pointer to a result structure, or NULL if no result is expected
 * @param argspec An argument specification string, see spec.md for details
 * @param args The arguments to the function
 * @return rpc_result_code_t The result code of the call
 */
MOSAPI rpc_result_code_t rpc_simple_callv(rpc_server_stub_t *stub, u32 funcid, rpc_result_t *result, const char *argspec, va_list args);

/**
 * @brief Manually create a new RPC call
 *
 * @param server The server stub to call
 * @param function_id The function ID to call
 * @return rpc_call_t* A pointer to the new call.
 */
MOSAPI rpc_call_t *rpc_call_create(rpc_server_stub_t *server, u32 function_id);

/**
 * @brief Add an argument to a call
 *
 * @param call The call to add the argument to
 * @param data A pointer to the argument data
 * @param size The size of the argument data
 */
MOSAPI void rpc_call_arg(rpc_call_t *call, const void *data, size_t size);

/**
 * @brief Execute a call
 *
 * @param call The call to execute
 * @param result_data A pointer to a pointer to the result data, or NULL if no result is expected
 * @param result_size A pointer to the size of the result data, or NULL if no result is expected
 * @return rpc_result_code_t The result code of the call
 *
 * @note The result data will be malloc'd and must be freed by the caller.
 */
MOSAPI rpc_result_code_t rpc_call_exec(rpc_call_t *call, void **result_data, size_t *result_size);

/**
 * @brief Destroy a call
 *
 * @param call The call to destroy
 */
MOSAPI void rpc_call_destroy(rpc_call_t *call);

typedef struct pb_msgdesc_s pb_msgdesc_t;

/**
 * @brief Call a function on the server using protobuf (nanopb)
 *
 * @param stub The server stub to call
 * @param funcid The function ID to call
 * @param reqm The protobuf message descriptor for the request
 * @param req The request message
 * @param respm The protobuf message descriptor for the response
 * @param resp The response message
 * @return rpc_result_code_t
 */
MOSAPI rpc_result_code_t rpc_do_pb_call(rpc_server_stub_t *stub, u32 funcid, const pb_msgdesc_t *reqm, const void *req, const pb_msgdesc_t *respm, void *resp);

#define rpc_pb_call(stub, funcid, reqt, req, respt, resp) rpc_do_pb_call(stub, funcid, reqt##_fields, req, respt##_fields, resp)
