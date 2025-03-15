// SPDX-License-Identifier: GPL-3.0-or-later

#include "rpc/rpc.hpp"

#include "ServiceManager.hpp"
#include "proto/services.pb.h"
#include "unit/unit.hpp"

#include <cstdlib>
#include <vector>

RpcUnitStatus GetUnitStatus(const Unit *unit)
{
    switch (unit->GetStatus())
    {
        case UnitStatus::Starting:
        case UnitStatus::Stopped:
        case UnitStatus::Exited:
        {
            return RpcUnitStatus_Stopped;
        }
        case UnitStatus::Stopping:
        case UnitStatus::Running:
        {
            return RpcUnitStatus_Running;
        }
        case UnitStatus::Failed:
        {
            return RpcUnitStatus_Failed;
        }
        case UnitStatus::Finished:
        {
            return RpcUnitStatus_Finished;
        }
    }
    __builtin_unreachable();
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
