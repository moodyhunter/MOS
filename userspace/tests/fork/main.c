// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/stdio.h"
#include "mos/syscall/usermode.h"

#define WAITMSG "pid %ld waits for %ld\n"

int main(void)
{
    int pid = syscall_fork();
    if (pid == 0)
    {
        printf("Child process: pid = %ld\n", syscall_get_pid());
        pid_t p1 = syscall_fork();
        if (p1)
        {
            printf(WAITMSG, syscall_get_pid(), p1);
            syscall_wait_for_process(p1);
        }

        pid_t p2 = syscall_fork();
        if (p2)
        {
            printf(WAITMSG, syscall_get_pid(), p2);
            syscall_wait_for_process(p2);
        }

        pid_t p3 = syscall_fork();
        if (p3)
        {
            printf(WAITMSG, syscall_get_pid(), p3);
            syscall_wait_for_process(p3);
        }

        pid_t p4 = syscall_fork();
        if (p4)
        {
            printf(WAITMSG, syscall_get_pid(), p4);
            syscall_wait_for_process(p4);
        }

        pid_t p5 = syscall_fork();
        if (p5)
        {
            printf(WAITMSG, syscall_get_pid(), p5);
            syscall_wait_for_process(p5);
        }

        pid_t p6 = syscall_fork();
        if (p6)
        {
            printf(WAITMSG, syscall_get_pid(), p6);
            syscall_wait_for_process(p6);
        }

        pid_t p7 = syscall_fork();
        if (p7)
        {
            printf(WAITMSG, syscall_get_pid(), p7);
            syscall_wait_for_process(p7);
        }
    }
    else
    {
        printf("Parent process: pid = %ld, child pid = %d\n", syscall_get_pid(), pid);
        syscall_wait_for_process(pid);
        printf("fork test passed\n");
    }
    return 0;
}
