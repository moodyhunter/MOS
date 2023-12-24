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
#include <mos_stdio.h>

int main(int argc, const char *argv[])
{
    MOS_UNUSED(argc);

    pid_t filesystem_server_pid = syscall_fork();
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

    return syscall_execveat(FD_CWD, "/initrd/programs/init", argv, NULL, 0);
}
