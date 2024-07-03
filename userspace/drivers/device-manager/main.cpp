// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm_common.hpp"
#include "dm_server.hpp"

#include <argparse/libargparse.h>
#include <iostream>
#include <libconfig/libconfig.h>
#include <librpc/rpc_server.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const argparse_arg_t dm_args[] = {
    { "help", 'h', ARGPARSE_NONE, "show this help message and exit" },
    { "config", 'c', ARGPARSE_REQUIRED, "path to the config file" },
    {},
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

    if (const auto result = Config::from_file(config_path); result)
        dm_config = *result;
    else
    {
        std::cerr << "Failed to parse config file: " << config_path << std::endl;
        return 1;
    }

    DeviceManagerServer dm_server;

    if (!start_load_drivers())
    {
        fputs("Failed to start device drivers\n", stderr);
        return 2;
    }
    dm_server.run();
    fputs("device_manager: server exited\n", stderr);

    return 0;
}
