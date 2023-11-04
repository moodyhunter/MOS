// SPDX-License-Identifier: GPL-3.0-or-later

#include "dm.h"
#include "dm/common.h"

#include <argparse/libargparse.h>
#include <libconfig/libconfig.h>
#include <librpc/rpc_server.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

const config_t *dm_config = NULL;

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

    dm_config = config_parse_file(config_path);
    if (!dm_config)
    {
        fprintf(stderr, "Failed to parse config file: %s\n", config_path);
        return 1;
    }

    rpc_server_t *server = rpc_server_create(MOS_DEVICE_MANAGER_SERVICE_NAME, NULL);

    if (!start_load_drivers(dm_config))
    {
        fputs("Failed to start device drivers\n", stderr);
        return 2;
    }

    dm_run_server(server);
    return 0;
}
