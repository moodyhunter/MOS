// SPDX-License-Identifier: GPL-3.0-or-later

#include "libsm.h"

#include "proto/services.pb.h"
#include "proto/services.service.h"

#include <cerrno>
#include <cstdlib>
#include <iostream>
#include <memory>

static constexpr auto UNIT_STATE_RECEIVER_SERVICE_SERVERNAME = "mos.service_manager.unit_state_receiver";

static_assert(static_cast<int>(UnitStatus::Failed) == static_cast<int>(RpcUnitStatusEnum_Failed));
static_assert(static_cast<int>(UnitStatus::Started) == static_cast<int>(RpcUnitStatusEnum_Started));
static_assert(static_cast<int>(UnitStatus::Starting) == static_cast<int>(RpcUnitStatusEnum_Starting));
static_assert(static_cast<int>(UnitStatus::Stopping) == static_cast<int>(RpcUnitStatusEnum_Stopping));
static_assert(static_cast<int>(UnitStatus::Stopped) == static_cast<int>(RpcUnitStatusEnum_Stopped));

bool ReportServiceState(UnitStatus status, const char *message)
{
    static const auto StateReceiver = std::make_unique<UnitStateReceiverStub>(UNIT_STATE_RECEIVER_SERVICE_SERVERNAME);
    static const char *ServiceToken = std::getenv("MOS_SERVICE_TOKEN");
    if (!ServiceToken)
    {
        std::cerr << "MOS_SERVICE_TOKEN not set" << std::endl;
        return false;
    }

    // unset MOS_SERVICE_TOKEN to avoid passing it to the service
    unsetenv("MOS_SERVICE_TOKEN");

    const auto rpcStatus = static_cast<RpcUnitStatusEnum>(status);
    const UnitStateNotifyRequest req{
        .service_id = const_cast<char *>(ServiceToken),
        .status =
            RpcUnitStatus{
                .isActive = rpcStatus != RpcUnitStatusEnum_Stopped,
                .status = rpcStatus,
                .statusMessage = const_cast<char *>(message),
                .timestamp = 0,
            },
    };

    UnitStateNotifyResponse resp;
    const auto ok = StateReceiver->notify(&req, &resp);
    if (!ok)
    {
        std::cerr << "Failed to notify service state for program: " << program_invocation_name << std::endl;
        return false;
    }

    return resp.success;
}
