// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "proto/services.service.h"

#include <librpc/rpc.h>
#include <memory>

constexpr auto SERVICE_MANAGER_RPC_NAME = "mos.service_manager";

class ServiceManagerServer : public IServiceManagerService
{
  public:
    explicit ServiceManagerServer(const std::string &rpcname) : IServiceManagerService(rpcname) {};

  private:
    rpc_result_code_t get_units(rpc_context_t *ctx, const GetUnitsRequest *req, GetUnitsResponse *resp) override;
    rpc_result_code_t get_templates(rpc_context_t *ctx, const GetTemplatesRequest *req, GetTemplatesResponse *resp) override;
    rpc_result_code_t start_unit(rpc_context_t *ctx, const StartUnitRequest *req, StartUnitResponse *resp) override;
    rpc_result_code_t stop_unit(rpc_context_t *ctx, const StopUnitRequest *req, StopUnitResponse *resp) override;
    rpc_result_code_t instantiate_unit(rpc_context_t *ctx, const InstantiateUnitRequest *req, InstantiateUnitResponse *resp) override;
    rpc_result_code_t get_unit_overrides(rpc_context_t *ctx, const GetUnitOverridesRequest *req, GetUnitOverridesResponse *resp) override;
};

inline const std::unique_ptr<ServiceManagerServer> RpcServer = std::make_unique<ServiceManagerServer>(SERVICE_MANAGER_RPC_NAME);
