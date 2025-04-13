// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "proto/services.service.h"

#include <librpc/rpc.h>
#include <memory>

#define UNIT_STATE_RECEIVER_SERVICE_SERVERNAME "mos.service_manager.unit_state_receiver"

class UnitStateReceiverServiceImpl : public IUnitStateReceiverService
{
  public:
    using IUnitStateReceiverService::IUnitStateReceiverService;
    virtual rpc_result_code_t notify(rpc_context_t *, UnitStateNotifyRequest *req, UnitStateNotifyResponse *res) override;
};

inline const auto UnitStateReceiverService = std::make_unique<UnitStateReceiverServiceImpl>(UNIT_STATE_RECEIVER_SERVICE_SERVERNAME);
