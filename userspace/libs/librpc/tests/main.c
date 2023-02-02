// SPDX-License-Identifier: GPL-3.0-or-later

#include "librpc/rpc.h"
#include "mos/syscall/usermode.h"

#define LINRPC_TEST_SERVERNAME "testserver"

enum
{
    TESTSERVER_ECHO = 0,
};

static int testserver_echo(rpc_server_t *server, rpc_request_t *request, rpc_reply_t **reply)
{
    MOS_UNUSED(server);
    MOS_UNUSED(request);
    MOS_UNUSED(reply);

    return 0;
}

void run_server(void)
{
    static rpc_function_info_t testserver_functions[] = {
        { TESTSERVER_ECHO, testserver_echo, 0 },
    };

    rpc_server_t *server = rpc_create_server(LINRPC_TEST_SERVERNAME, NULL);

    for (size_t i = 0; i < MOS_ARRAY_SIZE(testserver_functions); i++)
        rpc_register_function(server, testserver_functions[i]);

    rpc_server_run(server);
}

void run_client(void)
{
}

int main(int argc, char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    pid_t forked_pid = syscall_fork();
    if (forked_pid == 0)
        run_server();
    else
    {
        syscall_fork();
        syscall_fork();
        syscall_fork();

        // after 3 forks, we should have 8 child processes
        run_client();
    }

    return 0;
}
