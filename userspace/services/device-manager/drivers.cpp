// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm_common.hpp"
#include "proto/services.pb.h"
#include "proto/services.service.h"

#include <iostream>
#include <memory>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

constexpr auto SERVICE_MANAGER_RPC_NAME = "mos.service_manager";
const auto ServiceManager = std::make_shared<ServiceManagerStub>(SERVICE_MANAGER_RPC_NAME);

bool try_start_driver(u16 vendor, u16 device, u8 busid, u8 devid, u8 funcid, u64 mmio_base)
{
    char vendor_str[7];
    char device_str[7];
    snprintf(vendor_str, sizeof(vendor_str), "0x%04x", vendor);
    snprintf(device_str, sizeof(device_str), "0x%04x", device);

    InstantiateUnitRequest req;
    InstantiateUnitResponse resp;

    req.template_id = strdup("pci.device-template");
    req.parameters_count = 5;
    req.parameters = static_cast<mosrpc_KeyValuePair *>(malloc(5 * sizeof(mosrpc_KeyValuePair)));
    req.parameters[0].name = strdup("vendor_id");
    req.parameters[0].value = strdup(vendor_str);
    req.parameters[1].name = strdup("device_id");
    req.parameters[1].value = strdup(device_str);
    req.parameters[2].name = strdup("bus");
    req.parameters[2].value = strdup(std::to_string(busid).c_str());
    req.parameters[3].name = strdup("dev");
    req.parameters[3].value = strdup(std::to_string(devid).c_str());
    req.parameters[4].name = strdup("func");
    req.parameters[4].value = strdup(std::to_string(funcid).c_str());

    std::cout << "Instantiating unit for device: " << vendor_str << ":" << device_str << " at bus " << (int) busid << ", dev " << (int) devid << ", func " << (int) funcid
              << ", mmio_base " << std::hex << mmio_base << std::dec << std::endl;
    ServiceManager->instantiate_unit(&req, &resp);
    if (resp.unit_id == nullptr || resp.unit_id[0] == '\0')
    {
        std::cerr << "Failed to instantiate unit: " << req.template_id << std::endl;
        return false;
    }

    StartUnitRequest start_req{
        .unit_id = resp.unit_id,
    };
    StartUnitResponse start_resp;
    ServiceManager->start_unit(&start_req, &start_resp);
    if (!start_resp.success)
    {
        std::cerr << "Failed to start unit: " << req.template_id << std::endl;
        return false;
    }

    std::cout << "Successfully started unit: " << req.template_id << " with ID: " << resp.unit_id << std::endl;
    free(req.parameters[0].name);
    free(req.parameters[0].value);
    free(req.parameters[1].name);
    free(req.parameters[1].value);
    free(req.parameters[2].name);
    free(req.parameters[2].value);
    free(req.parameters[3].name);
    free(req.parameters[3].value);
    free(req.parameters[4].name);
    free(req.parameters[4].value);
    free(req.parameters);
    free(resp.unit_id);
    return true;
}
