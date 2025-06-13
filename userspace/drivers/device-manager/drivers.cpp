// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm_common.hpp"
#include "proto/services.pb.h"
#include "proto/services.service.h"

#include <iostream>
#include <libconfig/libconfig.h>
#include <memory>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

constexpr auto SERVICE_MANAGER_RPC_NAME = "mos.service_manager";
const auto ServiceManager = std::make_shared<ServiceManagerStub>(SERVICE_MANAGER_RPC_NAME);

bool start_load_drivers()
{
    const auto loads = dm_config.get_entries("loads");

    for (const auto &[_, driver] : loads)
    {
        pid_t driver_pid;
        posix_spawn(&driver_pid, driver.c_str(), NULL, NULL, NULL, NULL);
        if (driver_pid <= 0)
            std::cerr << "Failed to start driver: " << driver << std::endl;
    }

    return true;
}

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
    req.parameters = static_cast<KeyValuePair *>(malloc(5 * sizeof(KeyValuePair)));
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

    // first try to find a driver for the specific device
    char vendor_device_str[16];
    snprintf(vendor_device_str, sizeof(vendor_device_str), "%04x:%04x", vendor, device);
    auto driver_paths = dm_config.get_entry("drivers", vendor_device_str);

    // if that fails, try to find a driver for the vendor
    if (driver_paths.empty())
        driver_paths = dm_config.get_entry("drivers", vendor_str);

    // if the driver is still not found, leave
    if (driver_paths.empty())
        return false;

    if (driver_paths.size() > 1)
        puts("Multiple drivers found for device, using the first one");

    const auto driver_path = driver_paths[0].second;

    char location_str[16];
    char mmio_base_str[16];
    snprintf(location_str, sizeof(location_str), "%04x", busid << 16 | devid << 8 | funcid);
    snprintf(mmio_base_str, sizeof(mmio_base_str), "%llx", mmio_base);

    const char *argv[] = {
        driver_path.c_str(),                      //
        "--location",        location_str,        //
        "--mmio-base",       mmio_base_str, NULL, //
    };
    printf("Starting driver:");
    for (size_t i = 0; argv[i]; i++)
        printf(" %s", argv[i]);
    putchar('\n');

    pid_t driver_pid;
    posix_spawn(&driver_pid, driver_path.c_str(), NULL, NULL, (char *const *) argv, environ);

    return driver_pid > 0;
}
