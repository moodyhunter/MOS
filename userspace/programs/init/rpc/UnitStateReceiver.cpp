// SPDX-License-Identifier: GPL-3.0-or-later

#include "rpc/UnitStateReceiver.hpp"

#include "common/ConfigurationManager.hpp"

#include <librpc/rpc.h>

rpc_result_code_t UnitStateReceiverServiceImpl::notify(rpc_context_t *, const UnitStateNotifyRequest *req, UnitStateNotifyResponse *res)
{
    const auto token = req->service_id;
    const auto state = req->status;
    res->success = false;

    for (const auto &[unitid, unit] : ConfigurationManager->GetAllUnits())
    {
        if (unit->GetType() != UnitType::Service)
            continue;

        auto service = std::static_pointer_cast<Service>(unit);
        if (service->GetToken() != token)
            continue;

        UnitStatus newStatus;
        newStatus.active = state.isActive;
        newStatus.message = state.statusMessage;
        switch (state.status)
        {
            case RpcUnitStatusEnum_Starting: newStatus.status = UnitStatus::UnitStarting; break;
            case RpcUnitStatusEnum_Started: newStatus.status = UnitStatus::UnitStarted; break;
            case RpcUnitStatusEnum_Failed: newStatus.status = UnitStatus::UnitFailed; break;
            case RpcUnitStatusEnum_Stopping: newStatus.status = UnitStatus::UnitStopping; break;
            case RpcUnitStatusEnum_Stopped: newStatus.status = UnitStatus::UnitStopped; break;
            default:
            {
                std::cerr << "Unknown status" << std::endl;
                res->success = false;
                return RPC_RESULT_OK;
            }
        }

        service->ChangeState(newStatus);
        res->success = true;
        return RPC_RESULT_OK;
    }

    std::cerr << "Unable to find service with token " << token << std::endl;
    return RPC_RESULT_OK;
}
