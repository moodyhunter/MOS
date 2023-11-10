// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos_signal.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>

void sigint_handler(signal_t signum)
{
    printf("SIGINT(%d) received from PID %d, leaving...\n", (u32) signum, syscall_get_pid());
    exit(0);
}

void sigsegv_handler(signal_t signum)
{
    printf("SIGSEGV(%d) received from PID %d, leaving...\n", (u32) signum, syscall_get_pid());
    exit(0);
}

int main(int argc, const char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    register_signal_handler(SIGINT, sigint_handler);
    printf("Hello, world! (parent) PID=%d\n", syscall_get_pid());

    int child_pid = syscall_fork();
    if (child_pid == 0)
    {
        printf("Hello, world! (child) PID=%d\n", syscall_get_pid());
        while (1)
            puts("TOO BAD! SIGINT IS MISSING!");
    }

    kill(child_pid, SIGINT); // send SIGINT to child
    printf("Hehe muuurder go brrr\n");

    register_signal_handler(SIGSEGV, sigsegv_handler);

    volatile int *x = (void *) 0x01;
    *x = 10;

    puts("We should never reach this point");
    return 0;
}
