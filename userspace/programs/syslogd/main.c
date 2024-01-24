// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos-syslog.h"

#include <librpc/internal.h>
#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <librpc/rpc_server.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static rpc_server_t *server;

RPC_DECL_SERVER_PROTOTYPES(syslogd, SYSLOGD_RPC_X)

static rpc_result_code_t syslogd_set_name(rpc_server_t *server, rpc_context_t *context, void *data)
{
    MOS_UNUSED(server);
    const char *name = rpc_arg(context, 0, RPC_ARGTYPE_STRING, NULL);
    MOS_UNUSED(data);

    if (name == NULL)
        return RPC_RESULT_INVALID_ARGUMENT;

    printf("syslogd: setting name to '%s'\n", name);

    return RPC_RESULT_OK;
}

static rpc_result_code_t syslogd_log(rpc_server_t *server, rpc_context_t *context, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(data);

    const char *message = rpc_arg(context, 0, RPC_ARGTYPE_STRING, NULL);
    printf("%s\n", message);
    return RPC_RESULT_OK;
}

static rpc_result_code_t syslogd_logc(rpc_server_t *server, rpc_context_t *context, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(data);

    const char *category = rpc_arg(context, 0, RPC_ARGTYPE_STRING, NULL);
    const char *msg = rpc_arg(context, 1, RPC_ARGTYPE_STRING, NULL);
    printf("[%s] %s\n", category, msg);
    return RPC_RESULT_OK;
}

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    puts("syslogd: starting");
    server = rpc_server_create(SYSLOGD_SERVICE_NAME, NULL);
    rpc_server_register_functions(server, syslogd_functions, MOS_ARRAY_SIZE(syslogd_functions));
    rpc_server_exec(server);

    fputs("syslogd: server exited\n", stderr);
    return 0;
}
