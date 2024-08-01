// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "dm/dmrpc.h"

#include <libconfig/libconfig.h>
#include <librpc/rpc_server++.hpp>
#include <librpc/rpc_server.h>

RPC_DECL_SERVER_INTERFACE_CLASS(IDeviceManager, DEVICE_MANAGER_RPCS_X);

class DeviceManagerServer : public IDeviceManager
{
  public:
    DeviceManagerServer() : IDeviceManager(MOS_DEVICE_MANAGER_SERVICE_NAME) {};
    virtual rpc_result_code_t register_device(rpc_context_t *context, s32 vendor, s32 devid, s32 location, s64 mmio_base) override;
    virtual rpc_result_code_t register_driver(rpc_context_t *context) override;
};
