// SPDX-License-Identifier: GPL-3.0-or-later

#include "libsm.h"
#include "mos-syslog.h"

#include <librpc/internal.h>
#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <librpc/rpc_server.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

RPC_DECLARE_SERVER(syslogd, SYSLOGD_RPC_X)

static void syslogd_on_connect(rpc_context_t *context)
{
    const char *name = "<unknown>";
    rpc_context_set_data(context, strdup(name));
}

static void syslogd_on_disconnect(rpc_context_t *context)
{
    void *data = rpc_context_get_data(context);
    if (data != NULL)
        free(data); // client name
    MOS_UNUSED(context);
}

static rpc_result_code_t syslogd_set_name(rpc_context_t *context, const char *name)
{
    MOS_UNUSED(context);

    if (name == NULL)
        return RPC_RESULT_INVALID_ARGUMENT;

    printf("syslogd: setting name to '%s'\n", name);
    void *old = rpc_context_set_data(context, strdup(name));
    if (old != NULL)
        free(old);
    return RPC_RESULT_OK;
}

static rpc_result_code_t syslogd_log(rpc_context_t *context, const char *message)
{
    MOS_UNUSED(context);
    void *data = rpc_context_get_data(context);
    printf("[%s] %s\n", (char *) data, message);
    return RPC_RESULT_OK;
}

static rpc_result_code_t syslogd_logc(rpc_context_t *context, const char *category, const char *message)
{
    MOS_UNUSED(context);
    void *data = rpc_context_get_data(context);
    printf("[%s] [%s] %s\n", (char *) data, category, message);
    return RPC_RESULT_OK;
}

int main(int argc, char **argv)
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    puts("syslogd: starting");
    rpc_server_t *const server = rpc_server_create(SYSLOGD_SERVICE_NAME, NULL);
    rpc_server_set_on_connect(server, syslogd_on_connect);
    rpc_server_set_on_disconnect(server, syslogd_on_disconnect);
    rpc_server_register_functions(server, syslogd_functions, MOS_ARRAY_SIZE(syslogd_functions));

    ReportServiceState(UnitStatus::Started, "syslogd started");
    rpc_server_exec(server);
    fputs("syslogd: server exited\n", stderr);
    return 0;
}
