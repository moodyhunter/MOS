// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm.h"
#include "dm/common.h"

#include <libconfig/libconfig.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

bool start_load_drivers(const config_t *config)
{
    size_t num_drivers;
    const char **drivers = config_get_all(config, "load", &num_drivers);
    if (!drivers)
        return true; // no drivers to start

    for (size_t i = 0; i < num_drivers; i++)
    {
        const char *driver = drivers[i];
        pid_t driver_pid = shell_execute(driver);
        if (driver_pid <= 0)
            fprintf(stderr, "Failed to start driver: %s\n", driver);
    }

    free(drivers);
    return true;
}

bool try_start_driver(u16 vendor, u16 device, u32 location, u64 mmio_base)
{
    char vendor_str[5];
    char vendor_device_str[10];
    snprintf(vendor_str, sizeof(vendor_str), "%04x", vendor);
    snprintf(vendor_device_str, sizeof(vendor_device_str), "%04x:%04x", vendor, device);

    // first try to find a driver for the specific device
    const char *driver = config_get(dm_config, vendor_device_str);

    // if that fails, try to find a driver for the vendor
    if (!driver)
        driver = config_get(dm_config, vendor_str);

    // if the driver is still not found, leave
    if (!driver)
        return false;

    char *dup = strdup(driver);
    char *saveptr;

    char *driver_path = strtok_r(dup, " ", &saveptr);
    char *driver_args = strtok_r(NULL, " ", &saveptr);

    if (!driver_path)
        return false; // invalid options

    driver_path = string_trim(driver_path);

    if (unlikely(driver_args))
    {
        puts("argv for driver is not supported yet");
        free(dup);
        return false;
    }

    char device_str[16];
    char location_str[16];
    char mmio_base_str[16];
    snprintf(device_str, sizeof(device_str), "%04x", device);
    snprintf(location_str, sizeof(location_str), "%04x", location);
    snprintf(mmio_base_str, sizeof(mmio_base_str), "%llx", mmio_base);

    const char *argv[] = {
        driver_path,                  //
        "--vendor-id", vendor_str,    //
        "--device-id", device_str,    //
        "--location",  location_str,  //
        "--mmio-base", mmio_base_str, //
        NULL,
    };
    printf("Starting driver: ");
    for (size_t i = 0; argv[i]; i++)
        printf(" %s", argv[i]);
    putchar('\n');
    const pid_t driver_pid = spawn(driver_path, argv);
    free(dup);

    return driver_pid > 0;
}
