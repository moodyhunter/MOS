// SPDX-License-Identifier: GPL-3.0-or-later

#include "librpc/rpc_client.h"

#include <x86_console/client.h>

int main(void)
{
    rpc_server_stub_t *stub = open_console();
    console_simple_write(stub, "Hello, World!\n");

    while (1)
        ;
    return 0;
}
