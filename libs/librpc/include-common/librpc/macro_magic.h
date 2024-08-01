// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <mos/mos_global.h>

/**
 * @brief RPC macros for generating RPC client and server code.
 * @details The X_MACRO pattern is used here, RPC users should define their own
 *          X_MACROs to generate the code they need.
 *          The X_MACROs are called with the following arguments:
 *          - ARGS: A macro that generates function prototypes with specific arguments and types, this is called
 *                  with the following arguments:
 *              - prefix: the prefix of the function name, given by the third argument of the X_MACRO
 *              - id: the function ID of an RPC function, the user should not use duplicate IDs
 *              - name: the name of the function
 *              - NAME: the name of the function in uppercase, used to generate the enum values
 *              - spec: the specification of the function arguments, this is a string that contains
 *                      the types of the arguments, for example "iis" for int, int, string
 *                      read the spec.md file for more information.
 *              - ...:  the arguments of the function, this is a list of pairs of type and name
 *                      wrapped by ARG(TYPE, name), where TYPE is as defined in RPC_TYPE_XXX.
 *          - PB: A macro that generates function prototypes that use protobuf as the argument and
 *                return type, this is called with the following arguments:
 *              - prefix: the prefix of the function name, given by the third argument of the X_MACRO
 *              - id: the function ID of an RPC function, the user should not use duplicate IDs
 *              - name: the name of the function
 *              - NAME: the name of the function in uppercase, used to generate the enum values
 *              - reqtype: the type of the protobuf request
 *              - resptype: the type of the protobuf response
 *          - xarg: the user defined argument, this should be passed to the ARGS and PB as the first argument
 * @example
 * generate enum values for function IDs
 * #define MY_RPC_X(ARGS, PB, xarg) \
 *     ARGS(xarg, 0, foo, FOO, "i", ARG(int, x)) \
 *     PB(xarg, 1, bar, BAR, my_rpc_bar_request, my_rpc_bar_response)
 *
 * RPC_DEFINE_ENUMS(my_rpc, MY_RPC, MY_RPC_X) // optional
 *     this generates the enum values (MY_RPC_FOO = 0, MY_RPC_BAR = 1)
 *
 * RPC_CLIENT_DEFINE_SIMPLECALL(my_rpc, MY_RPC_X)
 *     this generates the simplecall implementation, which is a function that takes the arguments
 *     and calls the RPC server with the function ID and the arguments
 *     i.e.
 *       rpc_result_code_t my_rpc_foo(rpc_server_stub_t *server_stub, int x);
 *       rpc_result_code_t my_rpc_bar(rpc_server_stub_t *server_stub, my_rpc_bar_request *request, my_rpc_bar_response *response);
 *
 * RPC_DECL_SERVER_PROTOTYPES(my_rpc, MY_RPC_X)
 *     this generates the function prototypes and the function info array
 *     i.e.
 *       static int my_rpc_foo(rpc_context_t *ctx, ...args...);
 *       static int my_rpc_bar(rpc_context_t *ctx, my_rpc_bar_request *req, my_rpc_bar_response *resp);
 *       static const rpc_function_info_t my_rpc_functions[] = ...;
 *     which should be implemented by the user, and the function info array should be passed to
 *     \ref rpc_server_register_functions
 */

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
#define X_GENERATE_FUNCTION_STUB_IMPL_ARGS(prefix, id, name, NAME, spec, ...)                                                                                            \
    should_inline rpc_result_code_t prefix##name(rpc_server_stub_t *server_stub FOR_EACH(RPC_GENERATE_PROTOTYPE, __VA_ARGS__))                                           \
    {                                                                                                                                                                    \
        if (unlikely(spec == NULL))                                                                                                                                      \
            return RPC_RESULT_INVALID_ARGUMENT;                                                                                                                          \
        return rpc_simple_call(server_stub, id, NULL, spec FOR_EACH(RPC_EXTRACT_NAME, __VA_ARGS__));                                                                     \
    }

#define X_GENERATE_FUNCTION_STUB_IMPL_PB(prefix, id, name, NAME, reqtype, resptype)                                                                                      \
    should_inline rpc_result_code_t prefix##name(rpc_server_stub_t *server_stub, const reqtype *request, resptype *response)                                             \
    {                                                                                                                                                                    \
        return rpc_do_pb_call(server_stub, id, reqtype##_fields, request, resptype##_fields, response);                                                                  \
    }

// generate the simplecall implementation
#define RPC_CLIENT_DEFINE_SIMPLECALL(prefix, X_MACRO)                                                                                                                    \
    MOS_WARNING_PUSH                                                                                                                                                     \
    MOS_WARNING_DISABLE("-Wgnu-zero-variadic-macro-arguments")                                                                                                           \
    X_MACRO(X_GENERATE_FUNCTION_STUB_IMPL_ARGS, X_GENERATE_FUNCTION_STUB_IMPL_PB, prefix##_)                                                                             \
    MOS_WARNING_POP

// generate a stub class for C++ clients
#define X_GENERATE_FUNCTION_STUB_IMPL_CLASS_ARGS(prefix, id, name, NAME, spec, ...)                                                                                      \
    rpc_result_code_t prefix##name(FOR_EACH(RPC_GENERATE_PROTOTYPE, __VA_ARGS__))                                                                                        \
    {                                                                                                                                                                    \
        if (unlikely(spec == NULL))                                                                                                                                      \
            return RPC_RESULT_INVALID_ARGUMENT;                                                                                                                          \
        return rpc_simple_call(this->server_stub, id, NULL, spec FOR_EACH(RPC_EXTRACT_NAME, __VA_ARGS__));                                                               \
    }

#define X_GENERATE_FUNCTION_STUB_IMPL_CLASS_PB(prefix, id, name, NAME, reqtype, resptype)                                                                                \
    rpc_result_code_t prefix##name(const reqtype *request, resptype *response)                                                                                           \
    {                                                                                                                                                                    \
        return rpc_do_pb_call(this->server_stub, id, reqtype##_fields, request, resptype##_fields, response);                                                            \
    }

#define RPC_CLIENT_DEFINE_STUB_CLASS(_class_name, X_MACRO)                                                                                                               \
    class _class_name                                                                                                                                                    \
    {                                                                                                                                                                    \
      public:                                                                                                                                                            \
        explicit _class_name(const std::string &servername) : server_stub(rpc_client_create(servername.c_str())) {};                                                     \
        X_MACRO(X_GENERATE_FUNCTION_STUB_IMPL_CLASS_ARGS, X_GENERATE_FUNCTION_STUB_IMPL_CLASS_PB, );                                                                     \
        ~_class_name()                                                                                                                                                   \
        {                                                                                                                                                                \
            rpc_client_destroy(server_stub);                                                                                                                             \
        }                                                                                                                                                                \
        rpc_server_stub_t *get() const                                                                                                                                   \
        {                                                                                                                                                                \
            return server_stub;                                                                                                                                          \
        }                                                                                                                                                                \
                                                                                                                                                                         \
      private:                                                                                                                                                           \
        rpc_server_stub_t *server_stub;                                                                                                                                  \
    };

#define FOR_EACH_ARG_N(_1, _2, _3, _4, _5, _6, _7, _8, N, ...) N

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
#define FOR_EACH_RSEQ_N()        8, 7, 6, 5, 4, 3, 2, 1, 0
#define FOR_EACH_NARG_(...)      EXPAND(FOR_EACH_ARG_N(__VA_ARGS__))
#define FOR_EACH_NARG(...)       FOR_EACH_NARG_(__VA_ARGS__, FOR_EACH_RSEQ_N())
#define FOR_EACH_(N, what, ...)  EXPAND(MOS_CONCAT(FOR_EACH_, N)(what, __VA_ARGS__))
#define FOR_EACH(what, ...)      __VA_OPT__(FOR_EACH_(FOR_EACH_NARG(__VA_ARGS__), what, __VA_ARGS__))

#define X_EXTRACT_NAME_ARG(type, name) , name
#define RPC_EXTRACT_NAME(y)            X_EXTRACT_NAME_##y

#define _RPC_ARGTYPE_UINT8  u8
#define _RPC_ARGTYPE_UINT16 u16
#define _RPC_ARGTYPE_UINT32 u32
#define _RPC_ARGTYPE_UINT64 u64
#define _RPC_ARGTYPE_INT8   s8
#define _RPC_ARGTYPE_INT16  s16
#define _RPC_ARGTYPE_INT32  s32
#define _RPC_ARGTYPE_INT64  s64
#define _RPC_ARGTYPE_STRING const char *
#define _RPC_ARGTYPE_BUFFER const void *

#define _RPC_GETARG_UINT8  u8
#define _RPC_GETARG_UINT16 u16
#define _RPC_GETARG_UINT32 u32
#define _RPC_GETARG_UINT64 u64
#define _RPC_GETARG_INT8   s8
#define _RPC_GETARG_INT16  s16
#define _RPC_GETARG_INT32  s32
#define _RPC_GETARG_INT64  s64
#define _RPC_GETARG_STRING string
#define _RPC_GETARG_BUFFER buffer

#define X_GENERATE_PROTOTYPE_ARG(type, name) , __maybe_unused _RPC_ARGTYPE_##type name
#define RPC_GENERATE_PROTOTYPE(y)            X_GENERATE_PROTOTYPE_##y

// ============ SERVER SIDE ============
#define X_RPC_DO_GET_ARG(type, name) _RPC_ARGTYPE_##type name = MOS_CONCAT(rpc_arg_next_, EXPAND(_RPC_GETARG_##type))(context);
#define X_RPC_GET_ARG(arg)           X_RPC_DO_GET_##arg

#define X_GENERATE_FUNCTION_FORWARDS_ARGS(prefix, _id, _func, _FUNC, _spec, ...)                                                                                         \
    static rpc_result_code_t prefix##_func(rpc_context_t *context __VA_OPT__(FOR_EACH(RPC_GENERATE_PROTOTYPE, __VA_ARGS__)));                                            \
    static rpc_result_code_t prefix##_func##_wrapper(rpc_context_t *context)                                                                                             \
    {                                                                                                                                                                    \
        __VA_OPT__(FOR_EACH(X_RPC_GET_ARG, __VA_ARGS__))                                                                                                                 \
        return prefix##_func(context FOR_EACH(RPC_EXTRACT_NAME, __VA_ARGS__));                                                                                           \
    }

#define X_GENERATE_FUNCTION_FORWARDS_PB(prefix, _id, _func, _FUNC, reqtype, resptype)                                                                                    \
    static rpc_result_code_t prefix##_func(rpc_context_t *context, reqtype *req, resptype *resp);                                                                        \
    static rpc_result_code_t prefix##_func##_pb_wrapper(rpc_context_t *context)                                                                                          \
    {                                                                                                                                                                    \
        reqtype req = { 0 };                                                                                                                                             \
        if (!rpc_arg_pb(context, reqtype##_fields, &req, 0))                                                                                                             \
            return RPC_RESULT_SERVER_INTERNAL_ERROR;                                                                                                                     \
        resptype resp = resptype##_init_zero;                                                                                                                            \
        const rpc_result_code_t result = prefix##_func(context, &req, &resp);                                                                                            \
        pb_release(reqtype##_fields, &req);                                                                                                                              \
        rpc_write_result_pb(context, resptype##_fields, &resp);                                                                                                          \
        pb_release(resptype##_fields, &resp);                                                                                                                            \
        return result;                                                                                                                                                   \
    }

// clang-format off
#define X_NARGS_IMPL(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, N, ...) N
#define X_NARGS(...) X_NARGS_IMPL(_ __VA_OPT__(, ) __VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, _err)
// clang-format on

#define X_GENERATE_FUNCTION_INFO_ARGS_X(X)           X_GENERATE_FUNCTION_INFO_ARGS_X_##X
#define X_GENERATE_FUNCTION_INFO_ARGS_X_ARG(TYPE, _) RPC_ARGTYPE_##TYPE,
#define X_GENERATE_FUNCTION_INFO_ARGS_X_PB(TYPE, _)  /* unreachable */

#define X_DO_MAKE_FUNCINFO(_fid, _func, _nargs, _argtypes) { .function_id = _fid, .func = _func, .args_count = _nargs, .args_type = _argtypes },

#define X_DO_GENERATE_FUNCTION_INFO_ARGS(_fid, _func, _nargs, ...) X_DO_MAKE_FUNCINFO(_fid, _func, _nargs, { FOR_EACH(X_GENERATE_FUNCTION_INFO_ARGS_X, __VA_ARGS__) })
#define X_DO_GENERATE_FUNCTION_INFO_PB(_fid, _func)                X_DO_MAKE_FUNCINFO(_fid, _func, 1, { RPC_ARGTYPE_BUFFER })

#define X_GENERATE_FUNCTION_INFO_ARGS(prefix, fid, func, _1, _2, ...) X_DO_GENERATE_FUNCTION_INFO_ARGS(fid, prefix##func##_wrapper, X_NARGS(__VA_ARGS__), __VA_ARGS__)
#define X_GENERATE_FUNCTION_INFO_PB(prefix, fid, func, _1, _2, ...)   X_DO_GENERATE_FUNCTION_INFO_PB(fid, prefix##func##_pb_wrapper)

// Generate the function prototypes and the function info array
#define RPC_DECL_SERVER_PROTOTYPES(prefix, X_MACRO)                                                                                                                      \
    MOS_WARNING_PUSH                                                                                                                                                     \
    MOS_WARNING_DISABLE("-Wgnu-zero-variadic-macro-arguments")                                                                                                           \
    X_MACRO(X_GENERATE_FUNCTION_FORWARDS_ARGS, X_GENERATE_FUNCTION_FORWARDS_PB, prefix##_)                                                                               \
    static const rpc_function_info_t prefix##_functions[] = { X_MACRO(X_GENERATE_FUNCTION_INFO_ARGS, X_GENERATE_FUNCTION_INFO_PB, prefix##_) };                          \
    MOS_WARNING_POP

// ============ cpp class wrapper ============
#define X_GENERATE_NULL_FUNC_INFO_ARGS(unused, fid, func, _1, _2, ...) X_DO_GENERATE_FUNCTION_INFO_ARGS(fid, nullptr, X_NARGS(__VA_ARGS__), __VA_ARGS__)
#define X_GENERATE_NULL_FUNC_INFO_PB(unused, fid, func, _1, _2, ...)   X_DO_GENERATE_FUNCTION_INFO_PB(fid, nullptr)

#define X_GENERATE_SWITCH_CASE_CLASS_ARGS(unused, id, name, NAME, spec, ...)                                                                                             \
    case id:                                                                                                                                                             \
    {                                                                                                                                                                    \
        __VA_OPT__(FOR_EACH(X_RPC_GET_ARG, __VA_ARGS__))                                                                                                                 \
        return this->name(context FOR_EACH(RPC_EXTRACT_NAME, __VA_ARGS__));                                                                                              \
    }

#define X_GENERATE_SWITCH_CASE_CLASS_PB(unused, id, name, NAME, reqtype, resptype)                                                                                       \
    case id:                                                                                                                                                             \
    {                                                                                                                                                                    \
        reqtype req;                                                                                                                                                     \
        if (!rpc_arg_pb(context, reqtype##_fields, &req, 0))                                                                                                             \
            return RPC_RESULT_SERVER_INTERNAL_ERROR;                                                                                                                     \
        resptype resp = resptype##_init_zero;                                                                                                                            \
        const rpc_result_code_t result = this->name(context, &req, &resp);                                                                                               \
        pb_release(reqtype##_fields, &req);                                                                                                                              \
        rpc_write_result_pb(context, resptype##_fields, &resp);                                                                                                          \
        pb_release(resptype##_fields, &resp);                                                                                                                            \
        return result;                                                                                                                                                   \
    }

#define __X_GENERATE_FUNCTION_FORWARDS_ARGS_CPP_CLASS(_prefix, _id, func, _FUNC, _spec, ...)                                                                             \
    virtual rpc_result_code_t func(__maybe_unused rpc_context_t *ctx __VA_OPT__(FOR_EACH(RPC_GENERATE_PROTOTYPE, __VA_ARGS__)))                                          \
    {                                                                                                                                                                    \
        return rpc_result_code_t::RPC_RESULT_NOT_IMPLEMENTED;                                                                                                            \
    };

#define __X_GENERATE_FUNCTION_FORWARDS_PB_CPP_CLASS(_prefix, _id, func, _FUNC, reqtype, resptype)                                                                        \
    virtual rpc_result_code_t func(rpc_context_t *, reqtype *, resptype *)                                                                                               \
    {                                                                                                                                                                    \
        return rpc_result_code_t::RPC_RESULT_NOT_IMPLEMENTED;                                                                                                            \
    }

#define __RPC_DEFINE_SERVER_CPP_CLASS_WRAPPER(X_MACRO)                                                                                                                   \
    X_MACRO(__X_GENERATE_FUNCTION_FORWARDS_ARGS_CPP_CLASS, __X_GENERATE_FUNCTION_FORWARDS_PB_CPP_CLASS, )                                                                \
    virtual rpc_result_code_t dispatcher(rpc_context_t *context, u32 funcid) override final                                                                              \
    {                                                                                                                                                                    \
        switch (funcid)                                                                                                                                                  \
        {                                                                                                                                                                \
            X_MACRO(X_GENERATE_SWITCH_CASE_CLASS_ARGS, X_GENERATE_SWITCH_CASE_CLASS_PB, )                                                                                \
            default: __builtin_unreachable();                                                                                                                            \
        }                                                                                                                                                                \
    }

#define RPC_DECL_SERVER_INTERFACE_CLASS(classname, X_MACRO)                                                                                                              \
    class classname : public RPCServer                                                                                                                                   \
    {                                                                                                                                                                    \
        constexpr const static rpc_function_info_t rpc_functions[] = { X_MACRO(X_GENERATE_NULL_FUNC_INFO_ARGS, X_GENERATE_NULL_FUNC_INFO_PB, _) };                       \
                                                                                                                                                                         \
      public:                                                                                                                                                            \
        explicit classname(const std::string &rpcname) : RPCServer(rpcname, rpc_functions, MOS_ARRAY_SIZE(rpc_functions)) {};                                            \
        virtual ~classname() {};                                                                                                                                         \
        __RPC_DEFINE_SERVER_CPP_CLASS_WRAPPER(X_MACRO)                                                                                                                   \
    }

// ============ CPP TYPE NAMESPACE ============
#define __RPC_DO_DECL_CPP_TYPE_NAMESPACE_ALIAS_IMPL(subns, _id, action, ...)                                                                                             \
    namespace action                                                                                                                                                     \
    {                                                                                                                                                                    \
        using request = mos_rpc##_##subns##_##action##_request;                                                                                                          \
        using response = mos_rpc##_##subns##_##action##_response;                                                                                                        \
    }

#ifdef __cplusplus
#define RPC_DECL_CPP_TYPE_NAMESPACE(subns, X_MACRO)                                                                                                                      \
    namespace mos_rpc::subns                                                                                                                                             \
    {                                                                                                                                                                    \
        X_MACRO(__NO_OP, __RPC_DO_DECL_CPP_TYPE_NAMESPACE_ALIAS_IMPL, subns)                                                                                             \
    }
#else
#define RPC_DECL_CPP_TYPE_NAMESPACE(subns, X_MACRO)
#endif
