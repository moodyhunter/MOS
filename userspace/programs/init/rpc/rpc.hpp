// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "proto/services.service.h"

#include <memory>

constexpr auto SERVICE_MANAGER_RPC_NAME = "mos.service_manager";

class ServiceManagerServer : public IServiceManagerService
{
  public:
    explicit ServiceManagerServer(const std::string &rpcname) : IServiceManagerService(rpcname) {};

  private:
    virtual rpc_result_code_t get_units(rpc_context_t *ctx, GetUnitsRequest *req, GetUnitsResponse *resp) override;
};

inline const std::unique_ptr<ServiceManagerServer> RpcServer = std::make_unique<ServiceManagerServer>(SERVICE_MANAGER_RPC_NAME);
