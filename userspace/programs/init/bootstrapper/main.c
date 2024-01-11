// SPDX-License-Identifier: GPL-3.0-or-later
// A stage 1 init program for the MOS kernel.
//
// This program is responsible for:
// - Mounting the initrd
// - Starting the device manager, filesystem server
// - Starting the stage 2 init program

#include "bootstrapper.h"
#include "proto/filesystem.pb.h"

#include <librpc/rpc_server.h>
#include <mos/mos_global.h>
#include <stdio.h>
#include <unistd.h>

s64 strntoll(const char *str, char **endptr, int base, size_t n)
{
    s64 result = 0;
    bool negative = false;
    size_t i = 0;

    if (*str == '-')
        negative = true, str++, i++;
    else if (*str == '+')
        str++, i++;

    while (i < n && *str)
    {
        char c = *str;
        if (c >= '0' && c <= '9')
            result *= base, result += c - '0';
        else if (c >= 'a' && c <= 'z')
            result *= base, result += c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z')
            result *= base, result += c - 'A' + 10;
        else
            break;
        str++;
        i++;
    }
    if (endptr)
        *endptr = (char *) str;
    return negative ? -result : result;
}

int main(int argc, char *argv[])
{
    MOS_UNUSED(argc);

    pid_t filesystem_server_pid = fork();
    if (filesystem_server_pid == -1)
    {
        puts("bootstrapper: failed to fork filesystem server");
        return 0;
    }
    else if (filesystem_server_pid == 0)
    {
        puts("bootstrapper: starting filesystem server");
        cpiofs_run_server();
        puts("bootstrapper: filesystem server exited unexpectedly");
        return 0;
    }

    link("/lib", "/initrd/lib");

    return execv("/initrd/programs/init", argv);
}
