// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm/common.h"

#include <argparse/libargparse.h>
#include <libconfig/libconfig.h>
#include <librpc/rpc_server.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

extern void dm_run_server(rpc_server_t *server);

static bool start_device_drivers(const config_t *config)
{
    size_t num_drivers;
    const char **drivers = config_get_all(config, "load", &num_drivers);
    if (!drivers)
        return true; // no drivers to start

    for (size_t i = 0; i < num_drivers; i++)
    {
        const char *driver = drivers[i];

        char *dup = strdup(driver);
        char *saveptr;
        char *driver_path = strtok_r(dup, " ", &saveptr);
        char *driver_args = strtok_r(NULL, " ", &saveptr);

        driver_path = string_trim(driver_path);
        driver_args = string_trim(driver_args);

        if (!driver_path)
            return false; // invalid options

        size_t driver_args_count = 1;
        const char **driver_argv = malloc(driver_args_count * sizeof(char *));
        driver_argv[0] = driver_path;

        if (driver_args)
        {
            char *saveptr;
            char *arg = strtok_r(driver_args, " ", &saveptr);
            while (arg)
            {
                driver_args_count++;
                driver_argv = realloc(driver_argv, driver_args_count * sizeof(char *));
                driver_argv[driver_args_count - 1] = arg;
                arg = strtok_r(NULL, " ", &saveptr);
            }
        }

        driver_argv = realloc(driver_argv, (driver_args_count + 1) * sizeof(char *));
        driver_argv[driver_args_count] = NULL;

        pid_t driver_pid = spawn(driver_path, driver_argv);
        if (driver_pid <= 0)
            return false;
    }

    return true;
}

static const argparse_arg_t dm_args[] = {
    { "help", 'h', ARGPARSE_NONE, "show this help message and exit" },
    { "config", 'c', ARGPARSE_REQUIRED, "path to the config file" },
    { 0 },
};

int main(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    argparse_state_t arg_state;
    argparse_init(&arg_state, argv);

    const char *config_path = "/initrd/config/dm.conf";
    while (true)
    {
        const int option = argparse_long(&arg_state, dm_args, NULL);
        if (option == -1)
            break;

        switch (option)
        {
            case 'c': config_path = arg_state.optarg; break;
            case 'h': argparse_usage(&arg_state, dm_args, "device manager"); return 0;
            default: break;
        }
    }

    const config_t *dm_config = config_parse_file(config_path);
    if (!dm_config)
    {
        fprintf(stderr, "Failed to parse config file: %s\n", config_path);
        return 1;
    }

    rpc_server_t *server = rpc_server_create(MOS_DEVICE_MANAGER_SERVICE_NAME, NULL);

    if (!start_device_drivers(dm_config))
    {
        fputs("Failed to start device drivers\n", stderr);
        return 2;
    }

    dm_run_server(server);
    return 0;
}
