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

    if (getpid() != 1)
    {
        puts("bootstrapper: not running as PID 1, exiting");
        return 1;
    }

    int statusfd[2];
    pipe(statusfd);

    const pid_t filesystem_server_pid = fork();
    if (filesystem_server_pid == -1)
    {
        puts("bootstrapper: failed to fork filesystem server");
        return 1;
    }

    if (filesystem_server_pid == 0)
    {
        close(statusfd[0]);
        init_start_cpiofs_server(statusfd[1]);
        puts("bootstrapper: filesystem server exited unexpectedly");
        return 1;
    }

    // ! TODO: migrate to pthread_cond_wait after mlibc supports cross-process condition variables
    close(statusfd[1]);
    char buf[1] = { 0 };
    read(statusfd[0], buf, 1);
    close(statusfd[0]);
    if (buf[0] != 'v')
    {
        puts("bootstrapper: filesystem server failed to start");
        return 1;
    }

    if (link("/lib", "/initrd/lib") != 0)
    {
        puts("bootstrapper: failed to link /lib to /initrd/lib");
        return 1;
    }

    if (execv("/initrd/programs/init", argv) != 0)
    {
        puts("bootstrapper: failed to start init");
        return 1;
    }

    __builtin_unreachable();
}
