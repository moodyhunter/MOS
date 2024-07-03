// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm_common.hpp"

#include <iostream>
#include <libconfig/libconfig.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

bool try_start_driver(u16 vendor, u16 device, u32 location, u64 mmio_base)
{
    char vendor_str[5];
    char vendor_device_str[10];
    snprintf(vendor_str, sizeof(vendor_str), "%04x", vendor);
    snprintf(vendor_device_str, sizeof(vendor_device_str), "%04x:%04x", vendor, device);

    // first try to find a driver for the specific device
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

    char device_str[16];
    char location_str[16];
    char mmio_base_str[16];
    snprintf(device_str, sizeof(device_str), "%04x", device);
    snprintf(location_str, sizeof(location_str), "%04x", location);
    snprintf(mmio_base_str, sizeof(mmio_base_str), "%llx", mmio_base);

    const char *argv[] = {
        driver_path.c_str(),                      //
        "--vendor-id",       vendor_str,          //
        "--device-id",       device_str,          //
        "--location",        location_str,        //
        "--mmio-base",       mmio_base_str, NULL, //
    };
    printf("Starting driver:");
    for (size_t i = 0; argv[i]; i++)
        printf(" %s", argv[i]);
    putchar('\n');

    pid_t driver_pid;
    posix_spawn(&driver_pid, driver_path.c_str(), NULL, NULL, (char *const *) argv, NULL);

    return driver_pid > 0;
}
