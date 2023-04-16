// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

// ============ COMMON ============
// generate enum values for function IDs
#define RPC_DEFINE_ENUMS(name, NAME, X_MACRO)                                                                                                                            \
    MOS_WARNING_PUSH                                                                                                                                                     \
    MOS_WARNING_DISABLE("-Wgnu-zero-variadic-macro-arguments")                                                                                                           \
    typedef enum                                                                                                                                                         \
    {                                                                                                                                                                    \
        X_MACRO(X_GENERATE_ENUM, NAME##_)                                                                                                                                \
    } name##_rpc_functions;                                                                                                                                              \
    MOS_WARNING_POP
#define X_GENERATE_ENUM(prefix, id, name, NAME, ...) prefix##NAME = id,

// ============ CLIENT SIDE ============
// generate the simplecall implementation
#define RPC_CLIENT_DEFINE_SIMPLECALL(prefix, X_MACRO)                                                                                                                    \
    MOS_WARNING_PUSH                                                                                                                                                     \
    MOS_WARNING_DISABLE("-Wgnu-zero-variadic-macro-arguments")                                                                                                           \
    X_MACRO(X_GENERATE_FUNCTION_STUB_IMPL, prefix##_)                                                                                                                    \
    MOS_WARNING_POP

#define X_GENERATE_FUNCTION_STUB_IMPL(prefix, id, name, NAME, spec, ...)                                                                                                 \
    should_inline rpc_result_code_t prefix##name(rpc_server_stub_t *server_stub FOR_EACH(RPC_GENERATE_PROTOTYPE, __VA_ARGS__))                                           \
    {                                                                                                                                                                    \
        if (unlikely(spec == NULL))                                                                                                                                      \
        {                                                                                                                                                                \
            fprintf(stderr, "simple call isn't available for '" #name "'\n");                                                                                            \
            return RPC_RESULT_INVALID_ARG;                                                                                                                               \
        }                                                                                                                                                                \
        return rpc_simple_call(server_stub, id, NULL, spec FOR_EACH(RPC_EXTRACT_NAME, __VA_ARGS__));                                                                     \
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
#define DECLARE_RPC_SERVER_PROTOTYPES(name, X_MACRO)                                                                                                                     \
    MOS_WARNING_PUSH                                                                                                                                                     \
    MOS_WARNING_DISABLE("-Wgnu-zero-variadic-macro-arguments")                                                                                                           \
    X_MACRO(X_GENERATE_FUNCTION_FORWARDS, name##_)                                                                                                                       \
    static const rpc_function_info_t name##_functions[] = { X_MACRO(X_GENERATE_FUNCTION_INFO, name##_) };                                                                \
    MOS_WARNING_POP

// clang-format off
#define X_COUNT_ARGUMENTS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, N, ...) N
#define X_COUNT_ARGUMENTS(...) X_COUNT_ARGUMENTS_IMPL(_ __VA_OPT__(, ) __VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, _err)
#define X_GENERATE_FUNCTION_INFO(prefix, fid, function, FUNCTION, _, ...) { .function_id = fid, .func = prefix##function, .args_count = X_COUNT_ARGUMENTS(__VA_ARGS__) },
#define X_GENERATE_FUNCTION_FORWARDS(prefix, _id, _func, ...)   static int prefix##_func(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data);
// clang-format on
