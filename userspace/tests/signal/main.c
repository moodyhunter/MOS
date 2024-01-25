// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/tasks/signal_types.h>
#include <mos/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void sigint_handler(signal_t signum)
{
    printf("SIGINT(%d) received from PID %d, leaving...\n", (u32) signum, getpid());
    exit(0);
}

void sigsegv_handler(signal_t signum)
{
    printf("SIGSEGV(%d) received from PID %d, leaving...\n", (u32) signum, getpid());
    exit(0);
}

int main(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    signal(SIGINT, sigint_handler);
    printf("Hello, world! (parent) PID=%d\n", getpid());

    int child_pid = fork();
    if (child_pid == 0)
    {
        printf("Hello, world! (child) PID=%d\n", getpid());
        while (1)
            puts("TOO BAD! SIGINT IS MISSING!");
    }

    kill(child_pid, SIGINT); // send SIGINT to child
    printf("Hehe muuurder go brrr\n");

    signal(SIGSEGV, sigsegv_handler);

    volatile int *x = (void *) 0x01;
    *x = 10;

    puts("We should never reach this point");
    return 0;
}
