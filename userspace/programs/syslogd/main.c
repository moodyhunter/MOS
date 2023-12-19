// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos-syslog.h"

#include <librpc/macro_magic.h>
#include <librpc/rpc.h>
#include <librpc/rpc_server.h>
#include <mos_stdio.h>
#include <mos_string.h>

static rpc_server_t *server;

RPC_DECL_SERVER_PROTOTYPES(syslogd, SYSLOGD_RPC_X)

static const char *rpc_arg_next_string(rpc_args_iter_t *args)
{
    size_t message_len;
    const char *message = rpc_arg_next(args, &message_len);
    return message;
}

static int syslogd_set_name(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(args);
    MOS_UNUSED(reply);
    MOS_UNUSED(data);

    return RPC_RESULT_OK;
}

static int syslogd_log(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(reply);
    MOS_UNUSED(data);

    const char *message = rpc_arg_next_string(args);
    printf("%s\n", message);
    return RPC_RESULT_OK;
}

static int syslogd_logc(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(reply);
    MOS_UNUSED(data);

    const char *category = rpc_arg_next_string(args);
    const char *msg = rpc_arg_next_string(args);
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
