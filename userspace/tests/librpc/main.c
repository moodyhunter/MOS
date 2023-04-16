// SPDX-License-Identifier: GPL-3.0-or-later

#include "librpc/rpc.h"
#include "librpc/rpc_client.h"
#include "librpc/rpc_server.h"

#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RPC_TEST_SERVERNAME "testserver"

enum
{
    TESTSERVER_PING = 0,
    TESTSERVER_ECHO = 1,
    TESTSERVER_CALCULATE = 2,
    TESTSERVER_CLOSE = 3,
};

enum
{
    CALC_ADD = 0,
    CALC_SUB = 1,
    CALC_MUL = 2,
    CALC_DIV = 3,
};

static int testserver_ping(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(args);
    MOS_UNUSED(reply);
    MOS_UNUSED(data);
    printf("!!!!!!!!!!! ping !!!!!!!!!!!\n");
    return 0;
}

static int testserver_echo(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(data);

    size_t arg1_size = 0;
    const char *arg1 = rpc_arg_next(args, &arg1_size);
    printf("echo server: received '%.*s'\n", (int) arg1_size, arg1);
    rpc_write_result(reply, arg1, arg1_size);

    return 0;
}

static int testserver_calculation(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(data);

    size_t arg1_size;
    const int *a = rpc_arg_next(args, &arg1_size);
    size_t arg2_size;
    const int *b = rpc_arg_next(args, &arg2_size);
    size_t arg3_size;
    const int *c = rpc_arg_next(args, &arg3_size);

    int result = 0;
    switch (*b)
    {
        case CALC_ADD: result = *a + *c; break;
        case CALC_SUB: result = *a - *c; break;
        case CALC_MUL: result = *a * *c; break;
        case CALC_DIV: result = *a / *c; break;
    }

    static const char op_names[] = { '+', '-', '*', '/' };
    printf("calculation server: %d %c %d = %d\n", *a, op_names[*b], *c, result);

    rpc_write_result(reply, &result, sizeof(result));
    return 0;
}

static int rpc_server_close(rpc_server_t *server, rpc_args_iter_t *args, rpc_reply_t *reply, void *data)
{
    MOS_UNUSED(server);
    MOS_UNUSED(args);
    MOS_UNUSED(reply);
    MOS_UNUSED(data);

    printf("rpc_server_close\n");
    rpc_server_destroy(server);
    return 0;
}

void run_server(void)
{
    static const rpc_function_info_t testserver_functions[] = {
        { TESTSERVER_PING, testserver_ping, 0 },
        { TESTSERVER_ECHO, testserver_echo, 1 },
        { TESTSERVER_CALCULATE, testserver_calculation, 3 },
        { TESTSERVER_CLOSE, rpc_server_close, 0 },
    };

    rpc_server_t *server = rpc_server_create(RPC_TEST_SERVERNAME, NULL);
    rpc_server_register_functions(server, testserver_functions, MOS_ARRAY_SIZE(testserver_functions));
    rpc_server_exec(server);

    printf("rpc_server_destroy\n");
}

void run_client(void)
{
    rpc_server_stub_t *stub = rpc_client_create(RPC_TEST_SERVERNAME);

    if (!stub)
    {
        printf("rpc_client_create failed\n");
        return;
    }

    // ping
    {
        rpc_call_t *ping_call = rpc_call_create(stub, TESTSERVER_PING);
        rpc_call_exec(ping_call, NULL, NULL);
        rpc_call_destroy(ping_call);
    }

    // echo
    {
        rpc_call_t *echo_call = rpc_call_create(stub, TESTSERVER_ECHO);
        rpc_call_arg(echo_call, "hello world", 12);

        char *result;
        size_t result_size;
        rpc_call_exec(echo_call, (void *) &result, &result_size);
        rpc_call_destroy(echo_call);

        printf("echo client: received '%.*s'\n", (int) result_size, result);
        free(result);
    }

    // calculation
    {
        rpc_call_t *calc_call = rpc_call_create(stub, TESTSERVER_CALCULATE);
        int a = 10;
        int b = CALC_ADD;
        int c = 5;
        rpc_call_arg(calc_call, &a, sizeof(a));
        rpc_call_arg(calc_call, &b, sizeof(b));
        rpc_call_arg(calc_call, &c, sizeof(c));

        int *result;
        size_t result_size;
        rpc_call_exec(calc_call, (void *) &result, &result_size);
        rpc_call_destroy(calc_call);

        printf("calculation client: received '%d'\n", *result);
        free(result);

        rpc_call_t *calc_call2 = rpc_call_create(stub, TESTSERVER_CALCULATE);
        a = 10;
        b = CALC_SUB;
        c = 5;
        rpc_call_arg(calc_call2, &a, sizeof(a));
        rpc_call_arg(calc_call2, &b, sizeof(b));
        rpc_call_arg(calc_call2, &c, sizeof(c));

        rpc_call_exec(calc_call2, (void *) &result, &result_size);
        rpc_call_destroy(calc_call2);

        printf("calculation client: received '%d'\n", *result);
        free(result);
    }

    // calculation using argspec
    {
        rpc_result_t result;
        rpc_result_code_t result_code = rpc_simple_call(stub, TESTSERVER_CALCULATE, (void *) &result, "iii", 10, CALC_MUL, 5);
        printf("calculation client (spec): received '%d' (result_code=%d)\n", *(int *) result.data, result_code);
    }

    // close
    {
        rpc_simple_call(stub, TESTSERVER_CLOSE, NULL, "");
    }

    rpc_client_destroy(stub);
    printf("all done\n");
}

int main(int argc, char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    pid_t child = syscall_fork();
    if (child != 0)
    {
        run_server();
    }
    else
    {
        run_client();
    }

    return 0;
}
