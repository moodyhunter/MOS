// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/syscall/usermode.h>
#include <mos/tasks/signal_types.h>
#include <mos_signal.h>
#include <mos_stdio.h>

void sigint_handler(signal_t signum)
{
    printf("SIGINT(%d) received\n", (u32) signum);
}

int main(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    sigaction(SIGINT, sigint_handler);
    raise(SIGINT);

    puts("Hello, world!");

    return 0;
}
