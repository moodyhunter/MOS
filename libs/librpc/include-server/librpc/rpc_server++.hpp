// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#ifndef __cplusplus
#error "This header is only for C++"
#endif

#ifdef __MOS_KERNEL__
#error "This header is not for kernel"
#endif

#include "librpc/rpc_server.h"

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

        rpc_function_info_t *redirect_functions = (rpc_function_info_t *) alloca(sizeof(rpc_function_info_t) * count);
        for (size_t i = 0; i < count; i++)
        {
            redirect_functions[i] = functions[i];
            redirect_functions[i].func = redirector;
        }

        server = rpc_server_create(server_name.c_str(), this);
        rpc_server_register_functions(server, redirect_functions, count);
    }

    ~RPCServer()
    {
        rpc_server_close(server);
    }

    void run()
    {
        rpc_server_exec(server);
    }

    virtual rpc_result_code_t dispatcher(rpc_context_t *context, u32 funcid) = 0;

  private:
    rpc_server_t *server;
    std::string server_name;
};
