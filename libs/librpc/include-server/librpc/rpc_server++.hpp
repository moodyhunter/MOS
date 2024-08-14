// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#if defined(__MOS_KERNEL__) || !defined(__cplusplus)
#error "This file is only for use in C++ userspace code"
#endif

#include "librpc/rpc_server.h"

#include <pb_decode.h> // RPC_DECL_SERVER_INTERFACE_CLASS needs pb_decode.h
#include <string>

class RPCServer
{
  public:
    explicit RPCServer(const std::string &server_name, const rpc_function_info_t *functions, size_t count) : server_name(server_name)
    {
        const auto redirector = [](rpc_context_t *context)
        {
            const auto fid = rpc_context_get_function_id(context);
            const auto userdata = rpc_server_get_data(rpc_context_get_server(context));
            return ((RPCServer *) userdata)->dispatcher(context, fid);
        };

        const auto redirector_on_connect = [](rpc_context_t *context)
        {
            const auto userdata = rpc_server_get_data(rpc_context_get_server(context));
            ((RPCServer *) userdata)->on_connect(context);
        };

        const auto redirector_on_disconnect = [](rpc_context_t *context)
        {
            const auto userdata = rpc_server_get_data(rpc_context_get_server(context));
            ((RPCServer *) userdata)->on_disconnect(context);
        };

        rpc_function_info_t *redirect_functions = (rpc_function_info_t *) alloca(sizeof(rpc_function_info_t) * count);
        for (size_t i = 0; i < count; i++)
        {
            redirect_functions[i] = functions[i];
            redirect_functions[i].func = redirector;
        }

        server = rpc_server_create(server_name.c_str(), this);
        rpc_server_set_on_connect(server, redirector_on_connect);
        rpc_server_set_on_disconnect(server, redirector_on_disconnect);
        rpc_server_register_functions(server, redirect_functions, count);
    }

    virtual ~RPCServer()
    {
        rpc_server_close(server);
    }

    void run()
    {
        rpc_server_exec(server);
    }

    std::string get_name() const
    {
        return server_name;
    }

  protected:
    virtual rpc_result_code_t dispatcher(rpc_context_t *context, u32 funcid) = 0;

    virtual void on_connect(rpc_context_t *)
    {
    }

    virtual void on_disconnect(rpc_context_t *)
    {
    }

  protected:
    template<typename T>
    T *get_data(rpc_context_t *context)
    {
        return (T *) rpc_context_get_data(context);
    }

    template<typename T>
    void set_data(rpc_context_t *context, T *data)
    {
        rpc_context_set_data(context, data);
    }

  private:
    rpc_server_t *server;
    std::string server_name;
};
