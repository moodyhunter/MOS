// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// ============ COMMON ============
// generate enum values for function IDs
#define RPC_DEFINE_ENUMS(name, NAME, X_MACRO)                                                                                                                            \
    MOS_WARNING_PUSH                                                                                                                                                     \
    MOS_WARNING_DISABLE("-Wgnu-zero-variadic-macro-arguments")                                                                                                           \
    typedef enum                                                                                                                                                         \
    {                                                                                                                                                                    \
        X_MACRO(X_GENERATE_ENUM, X_GENERATE_ENUM, NAME##_)                                                                                                               \
    } name##_rpc_functions;                                                                                                                                              \
    MOS_WARNING_POP

#define X_GENERATE_ENUM(prefix, id, name, NAME, ...) prefix##NAME = id,

// ============ CLIENT SIDE ============
// generate the simplecall implementation
#define RPC_CLIENT_DEFINE_SIMPLECALL(prefix, X_MACRO)                                                                                                                    \
    MOS_WARNING_PUSH                                                                                                                                                     \
    MOS_WARNING_DISABLE("-Wgnu-zero-variadic-macro-arguments")                                                                                                           \
    X_MACRO(X_GENERATE_FUNCTION_STUB_IMPL_ARGS, X_GENERATE_FUNCTION_STUB_IMPL_PB, prefix##_)                                                                             \
    MOS_WARNING_POP

#define X_GENERATE_FUNCTION_STUB_IMPL_ARGS(prefix, id, name, NAME, spec, ...)                                                                                            \
    should_inline rpc_result_code_t prefix##name(rpc_server_stub_t *server_stub FOR_EACH(RPC_GENERATE_PROTOTYPE, __VA_ARGS__))                                           \
    {                                                                                                                                                                    \
        if (unlikely(spec == NULL))                                                                                                                                      \
            return RPC_RESULT_INVALID_ARG;                                                                                                                               \
        return rpc_simple_call(server_stub, id, NULL, spec FOR_EACH(RPC_EXTRACT_NAME, __VA_ARGS__));                                                                     \
    }

#define X_GENERATE_FUNCTION_STUB_IMPL_PB(prefix, id, name, NAME, reqtype, resptype)                                                                                      \
    should_inline rpc_result_code_t prefix##name(rpc_server_stub_t *server_stub, const void *request, void *response)                                                    \
    {                                                                                                                                                                    \
        return rpc_do_pb_call(server_stub, id, reqtype##_fields, request, resptype##_fields, response);                                                                  \
    }

#define EXPAND(x) x
#define FOR_EACH_0(what, ...)
#define FOR_EACH_1(what, x, ...) what(x) EXPAND(FOR_EACH_0(what, __VA_ARGS__))
#define FOR_EACH_2(what, x, ...) what(x) EXPAND(FOR_EACH_1(what, __VA_ARGS__))
#define FOR_EACH_3(what, x, ...) what(x) EXPAND(FOR_EACH_2(what, __VA_ARGS__))
#define FOR_EACH_4(what, x, ...) what(x) EXPAND(FOR_EACH_3(what, __VA_ARGS__))
#define FOR_EACH_5(what, x, ...) what(x) EXPAND(FOR_EACH_4(what, __VA_ARGS__))
#define FOR_EACH_6(what, x, ...) what(x) EXPAND(FOR_EACH_5(what, __VA_ARGS__))
#define FOR_EACH_7(what, x, ...) what(x) EXPAND(FOR_EACH_6(what, __VA_ARGS__))
#define FOR_EACH_8(what, x, ...) what(x) EXPAND(FOR_EACH_7(what, __VA_ARGS__))
#define FOR_EACH_9(what, x, ...) what(x) EXPAND(FOR_EACH_8(what, __VA_ARGS__))
#define FOR_EACH_NARG(...)       FOR_EACH_NARG_(__VA_ARGS__, FOR_EACH_RSEQ_N())
#define FOR_EACH_NARG_(...)      EXPAND(FOR_EACH_ARG_N(__VA_ARGS__))
#define FOR_EACH_RSEQ_N()        8, 7, 6, 5, 4, 3, 2, 1, 0
#define FOR_EACH_(N, what, ...)  EXPAND(MOS_CONCAT(FOR_EACH_, N)(what, __VA_ARGS__))
#define FOR_EACH(what, ...)      __VA_OPT__(FOR_EACH_(FOR_EACH_NARG(__VA_ARGS__), what, __VA_ARGS__))

#define FOR_EACH_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N

#define X_EXTRACT_NAME_ARG(type, name) , name
#define RPC_EXTRACT_NAME(y)            X_EXTRACT_NAME_##y

#define X_GENERATE_PROTOTYPE_ARG(type, name) , type name
#define RPC_GENERATE_PROTOTYPE(y)            X_GENERATE_PROTOTYPE_##y

// ============ SERVER SIDE ============
// Generate the function prototypes and the function info array
#define RPC_DECL_SERVER_PROTOTYPES(name, X_MACRO)                                                                                                                        \
    MOS_WARNING_PUSH                                                                                                                                                     \
    MOS_WARNING_DISABLE("-Wgnu-zero-variadic-macro-arguments")                                                                                                           \
    X_MACRO(X_GENERATE_FUNCTION_FORWARDS_ARGS, X_GENERATE_FUNCTION_FORWARDS_PB, name##_)                                                                                 \
    static const rpc_function_info_t name##_functions[] = { X_MACRO(X_GENERATE_FUNCTION_INFO_ARGS, X_GENERATE_FUNCTION_INFO_PB, name##_) };                              \
    MOS_WARNING_POP

#define X_GENERATE_FUNCTION_FORWARDS_ARGS(prefix, _id, _func, ...) static int prefix##_func(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data);

// clang-format off
#define X_COUNT_ARGUMENTS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, N, ...) N
#define X_COUNT_ARGUMENTS(...) X_COUNT_ARGUMENTS_IMPL(_ __VA_OPT__(, ) __VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, _err)
// clang-format on

#define X_DO_GENERATE_FUNCION_INFO(_fid, _func, _nargs) { .function_id = _fid, .func = _func, .args_count = _nargs },

#define X_GENERATE_FUNCTION_INFO_ARGS(prefix, fid, function, FUNCTION, _, ...) X_DO_GENERATE_FUNCION_INFO(fid, prefix##function, X_COUNT_ARGUMENTS(__VA_ARGS__))
#define X_GENERATE_FUNCTION_INFO_PB(prefix, fid, function, FUNCTION, _, ...)   X_DO_GENERATE_FUNCION_INFO(fid, prefix##function##_pb_wrapper, 1)

#define X_GENERATE_FUNCTION_FORWARDS_PB(prefix, _id, _func, _FUNC, reqtype, resptype)                                                                                    \
    static int prefix##_func(rpc_server_t *server, reqtype *req, resptype *resp, void *data);                                                                            \
    static int prefix##_func##_pb_wrapper(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)                                                   \
    {                                                                                                                                                                    \
        reqtype req = { 0 };                                                                                                                                             \
        if (!rpc_arg_next_pb(reqtype, req, args))                                                                                                                        \
            return RPC_RESULT_SERVER_INTERNAL_ERROR;                                                                                                                     \
        resptype resp = resptype##_init_zero;                                                                                                                            \
        int result = prefix##_func(server, &req, &resp, data);                                                                                                           \
        pb_release(reqtype##_fields, &req);                                                                                                                              \
        rpc_write_result_pb(resptype, resp, reply);                                                                                                                      \
        pb_release(resptype##_fields, &resp);                                                                                                                            \
        return result;                                                                                                                                                   \
    }
