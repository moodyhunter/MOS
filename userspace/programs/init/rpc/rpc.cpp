// SPDX-License-Identifier: GPL-3.0-or-later

#include "rpc/rpc.hpp"

#include "ServiceManager.hpp"
#include "proto/services.pb.h"
#include "unit/unit.hpp"

#include <chrono>
#include <cstdlib>
#include <vector>

static RpcUnitStatus GetUnitStatus(const Unit *unit)
{
    RpcUnitStatusEnum statusEnum;
    const auto status = unit->GetStatus();
    switch (status.status)
    {
        case UnitStatus::UnitStarting: statusEnum = RpcUnitStatusEnum_Starting; break;
        case UnitStatus::UnitStarted: statusEnum = RpcUnitStatusEnum_Started; break;
        case UnitStatus::UnitFailed: statusEnum = RpcUnitStatusEnum_Failed; break;
        case UnitStatus::UnitStopping: statusEnum = RpcUnitStatusEnum_Stopping; break;
        case UnitStatus::Invalid: statusEnum = RpcUnitStatusEnum_Stopped; break;
    }
    RpcUnitStatus rpcStatus{
        .isActive = status.active,
        .status = statusEnum,
        .statusMessage = strdup(status.message.c_str()),
        .timestamp = std::chrono::system_clock::to_time_t(status.timestamp),
    };

    return rpcStatus;
};

rpc_result_code_t ServiceManagerServer::get_units(rpc_context_t *ctx, GetUnitsRequest *req, GetUnitsResponse *resp)
{
    (void) ctx;
    (void) req;

    const auto units = ServiceManager->GetAllUnits();

    resp->units_count = units.size();
    resp->units = static_cast<RpcUnit *>(malloc(units.size() * sizeof(RpcUnit)));
    if (!resp->units)
        return RPC_RESULT_SERVER_INTERNAL_ERROR;

    for (size_t i = 0; i < units.size(); i++)
    {
        const auto &unit = units[i];
        resp->units[i].type = static_cast<RpcUnitType>(unit->GetType());
        resp->units[i].description = strdup(unit->description.c_str());
        resp->units[i].name = strdup(unit->id.c_str());
        resp->units[i].status = GetUnitStatus(unit.get());
    }

    return RPC_RESULT_OK;
}

rpc_result_code_t ServiceManagerServer::get_templates(rpc_context_t *ctx, GetTemplatesRequest *req, GetTemplatesResponse *resp)
{
    (void) ctx;
    (void) req;

    const auto templates = ServiceManager->GetAllTemplates();

    resp->templates_count = templates.size();
    resp->templates = static_cast<RpcUnitTemplate *>(malloc(templates.size() * sizeof(RpcUnitTemplate)));
    if (!resp->templates)
        return RPC_RESULT_SERVER_INTERNAL_ERROR;

    for (size_t i = 0; i < templates.size(); i++)
    {
        const auto &template_ = templates[i];
        resp->templates[i].base_id = strdup(template_->id.c_str());
        resp->templates[i].description = strdup(template_->table["description"].value_or("<unknown>"));

        const auto &params = template_->parameters;
        resp->templates[i].parameters_count = params.size();
        resp->templates[i].parameters = static_cast<char **>(malloc(params.size() * sizeof(char *)));
        if (!resp->templates[i].parameters)
            return RPC_RESULT_SERVER_INTERNAL_ERROR;
        for (size_t j = 0; j < params.size(); j++)
            resp->templates[i].parameters[j] = strdup(params[j].c_str());
    }

    return RPC_RESULT_OK;
}

rpc_result_code_t ServiceManagerServer::start_unit(rpc_context_t *ctx, StartUnitRequest *req, StartUnitResponse *resp)
{
    (void) ctx;
    resp->success = ServiceManager->StartUnit(req->unit_id);
    return RPC_RESULT_OK;
}

rpc_result_code_t ServiceManagerServer::stop_unit(rpc_context_t *ctx, StopUnitRequest *req, StopUnitResponse *resp)
{
    (void) ctx;
    resp->success = ServiceManager->StopUnit(req->unit_id);
    return RPC_RESULT_OK;
}
