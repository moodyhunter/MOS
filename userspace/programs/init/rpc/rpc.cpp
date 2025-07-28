// SPDX-License-Identifier: GPL-3.0-or-later

#include "rpc/rpc.hpp"

#include "ServiceManager.hpp"
#include "common/ConfigurationManager.hpp"
#include "proto/mosrpc.pb.h"
#include "proto/services.pb.h"
#include "units/inherited.hpp"
#include "units/template.hpp"
#include "units/unit.hpp"

#include <chrono>
#include <cstdlib>
#include <vector>

static RpcUnitStatus GetUnitStatus(const IUnit *unit)
{
    RpcUnitStatusEnum statusEnum;
    const auto status = unit->GetStatus();
    switch (status.status)
    {
        case UnitStatus::UnitStarting: statusEnum = RpcUnitStatusEnum_Starting; break;
        case UnitStatus::UnitStarted: statusEnum = RpcUnitStatusEnum_Started; break;
        case UnitStatus::UnitFailed: statusEnum = RpcUnitStatusEnum_Failed; break;
        case UnitStatus::UnitStopping: statusEnum = RpcUnitStatusEnum_Stopping; break;
        case UnitStatus::UnitStopped: statusEnum = RpcUnitStatusEnum_Stopped; break;
    }
    RpcUnitStatus rpcStatus{
        .isActive = status.active,
        .status = statusEnum,
        .statusMessage = strdup(status.message.c_str()),
        .timestamp = std::chrono::system_clock::to_time_t(status.timestamp),
    };

    return rpcStatus;
};

rpc_result_code_t ServiceManagerServer::get_units(rpc_context_t *ctx, const GetUnitsRequest *req, GetUnitsResponse *resp)
{
    (void) ctx;
    (void) req;

    const auto units = ConfigurationManager->GetAllUnits();
    int nUnits = 0;

    std::map<std::string, std::string> inheritedUnits; // child id -> base_id
    for (const auto &[id, unit] : units)
    {
        if (unit->GetType() == UnitType::Inherited)
        {
            const auto inheritedUnit = std::dynamic_pointer_cast<const InheritedUnit>(unit);
            if (inheritedUnit)
                inheritedUnits[inheritedUnit->GetChildId()] = id;
        }
        else
        {
            nUnits++;
        }
    }

    resp->units_count = nUnits;
    resp->units = static_cast<RpcUnit *>(malloc(nUnits * sizeof(RpcUnit)));
    if (!resp->units)
        return RPC_RESULT_SERVER_INTERNAL_ERROR;

    int i = 0;
    for (const auto &[id, unit] : units)
    {
        if (unit->GetType() == UnitType::Inherited)
            continue; // skip inherited units in the main list

        resp->units[i].type = static_cast<RpcUnitType>(unit->GetType());
        resp->units[i].description = strdup(unit->GetDescription().c_str());
        resp->units[i].name = strdup(unit->id.c_str());
        resp->units[i].status = GetUnitStatus(unit.get());

        std::vector<std::string> overrideChain;
        {
            bool isInherited = true;
            std::string baseId = id;
            while (isInherited)
            {
                isInherited = false;
                auto it = inheritedUnits.find(baseId);
                if (it != inheritedUnits.end() && it->second != baseId)
                {
                    isInherited = true;
                    overrideChain.push_back(it->second);
                    baseId = it->second;
                    continue;
                }
            }
        }

        resp->units[i].overridden_units_count = overrideChain.size();
        resp->units[i].overridden_units = static_cast<RpcOverriddenUnit *>(malloc((1 + overrideChain.size()) * sizeof(RpcOverriddenUnit)));
        if (!resp->units[i].overridden_units)
            return RPC_RESULT_SERVER_INTERNAL_ERROR;
        for (size_t j = 0; j < overrideChain.size(); j++)
        {
            resp->units[i].overridden_units[j].base_unit_id = strdup(overrideChain[j].c_str());
        }

        i++;
    }

    return RPC_RESULT_OK;
}

rpc_result_code_t ServiceManagerServer::get_templates(rpc_context_t *ctx, const GetTemplatesRequest *req, GetTemplatesResponse *resp)
{
    (void) ctx;
    (void) req;

    const auto templates = ConfigurationManager->GetAllTemplates();

    resp->templates_count = templates.size();
    resp->templates = static_cast<RpcUnitTemplate *>(malloc(templates.size() * sizeof(RpcUnitTemplate)));
    if (!resp->templates)
        return RPC_RESULT_SERVER_INTERNAL_ERROR;

    int i = 0;
    for (const auto &[id, template_] : templates)
    {
        resp->templates[i].base_id = strdup(template_->id.c_str());
        resp->templates[i].description = strdup(template_->table["description"].value_or("<unknown>"));
        {
            // parameters
            const auto &params = template_->GetParameters();
            resp->templates[i].parameters_count = params.size();
            resp->templates[i].parameters = static_cast<char **>(malloc(params.size() * sizeof(char *)));
            if (!resp->templates[i].parameters)
                return RPC_RESULT_SERVER_INTERNAL_ERROR;
            for (size_t j = 0; j < params.size(); j++)
                resp->templates[i].parameters[j] = strdup(params[j].c_str());
        }
        {
            const auto &predefined_args = template_->predefined_args;
            resp->templates[i].predefined_arguments_count = predefined_args.size();
            resp->templates[i].predefined_arguments = static_cast<mosrpc_KeyValuePair *>(malloc(predefined_args.size() * sizeof(mosrpc_KeyValuePair)));
            if (!resp->templates[i].predefined_arguments)
                return RPC_RESULT_SERVER_INTERNAL_ERROR;
            int j = 0;
            for (const auto &[key, value] : predefined_args)
            {
                resp->templates[i].predefined_arguments[j].name = strdup(key.c_str());
                resp->templates[i].predefined_arguments[j].value = strdup(value.c_str());
                j++;
            }
        }

        i++;
    }

    return RPC_RESULT_OK;
}

rpc_result_code_t ServiceManagerServer::start_unit(rpc_context_t *ctx, const StartUnitRequest *req, StartUnitResponse *resp)
{
    (void) ctx;
    resp->success = ServiceManager->StartUnit(req->unit_id);
    return RPC_RESULT_OK;
}

rpc_result_code_t ServiceManagerServer::stop_unit(rpc_context_t *ctx, const StopUnitRequest *req, StopUnitResponse *resp)
{
    (void) ctx;
    resp->success = ServiceManager->StopUnit(req->unit_id);
    return RPC_RESULT_OK;
}

rpc_result_code_t ServiceManagerServer::instantiate_unit(rpc_context_t *ctx, const InstantiateUnitRequest *req, InstantiateUnitResponse *resp)
{
    (void) ctx;
    ArgumentMap args;
    for (size_t i = 0; i < req->parameters_count; i++)
    {
        const auto &param = req->parameters[i];
        args[param.name] = param.value;
    }

    const auto unit = ConfigurationManager->InstantiateUnit(req->template_id, args);
    if (!unit)
        return RPC_RESULT_SERVER_INTERNAL_ERROR;

    resp->unit_id = strdup(unit->id.c_str());
    return RPC_RESULT_OK;
}

rpc_result_code_t ServiceManagerServer::get_unit_overrides(rpc_context_t *ctx, const GetUnitOverridesRequest *req, GetUnitOverridesResponse *resp)
{
    (void) ctx;
    (void) req;

    const auto overrides = ConfigurationManager->GetTemplateOverrides();

    resp->overrides_count = overrides.size();
    resp->overrides = static_cast<RpcUnitOverride *>(malloc(overrides.size() * sizeof(RpcUnitOverride)));
    if (!resp->overrides)
        return RPC_RESULT_SERVER_INTERNAL_ERROR;

    int i = 0;
    for (const auto &[pair, override] : overrides)
    {
        const auto &[id, args] = pair;
        resp->overrides[i].base_unit_id = strdup(id.c_str());
        resp->overrides[i].overrides_count = args.size();
        resp->overrides[i].overrides = static_cast<mosrpc_KeyValuePair *>(malloc(args.size() * sizeof(mosrpc_KeyValuePair)));
        if (!resp->overrides[i].overrides)
            return RPC_RESULT_SERVER_INTERNAL_ERROR;
        int j = 0;
        for (const auto &[key, value] : args)
        {
            resp->overrides[i].overrides[j].name = strdup(key.c_str());
            resp->overrides[i].overrides[j].value = strdup(value.c_str());
            j++;
        }
        resp->overrides[i].overridden_unit_id = strdup(override.c_str());
        i++;
    }

    return RPC_RESULT_OK;
}
