// SPDX-License-Identifier: GPL-3.0-or-later

#include "proto/net-networkd.service.h"

#include <iostream>
#include <librpc/rpc.h>
#include <libsm.h>
#include <memory>

constexpr auto NETWORKD_SERVICE_NAME = "mos.networkd";

class NetworkDaemonImpl : public INetworkManagerService
{
  public:
    explicit NetworkDaemonImpl() : INetworkManagerService(NETWORKD_SERVICE_NAME)
    {
        std::cout << program_invocation_name << ": " << "NetworkDaemonImpl constructor" << std::endl;
    }

  public:
    rpc_result_code_t register_network_device(rpc_context_t *, RegisterNetworkDeviceRequest *request, RegisterNetworkDeviceResponse *response) override
    {
        std::cerr << "Registering network device: " << request->device_name << " with server: " << request->rpc_server_name << std::endl;
        response->result.success = true;
        response->result.error = nullptr;
        return RPC_RESULT_OK;
    }
};

const auto NetworkDaemon = std::make_shared<NetworkDaemonImpl>();

int main(int, char **)
{
    std::cout << program_invocation_name << ": " << "Starting networkd" << std::endl;
    ReportServiceState(UnitStatus::Started, "started networkd");
    NetworkDaemon->run();

    ReportServiceState(UnitStatus::Stopping, "stopping networkd");

    ReportServiceState(UnitStatus::Stopped, "stopping networkd");
    return 0;
}
