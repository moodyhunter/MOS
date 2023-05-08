// SPDX-License-Identifier: GPL-3.0-or-later

#include <x86_console/client.h>

int main(int argc, char **argv)
{
    rpc_server_stub_t *stub = open_console();

    print_to_console("Hello, World!\n");
    print_to_console("You passed %d arguments:\n", argc);

    for (int i = 1; i < argc; i++)
        print_to_console("  %s\n", argv[i]);

    rpc_client_destroy(stub);
    return 0;
}
