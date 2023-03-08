// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/stdio.h"
#include "mos/syscall/usermode.h"

int main(void)
{
    int pid = syscall_fork();
    if (pid == 0)
    {
        printf("Child process: pid = %ld\n", syscall_get_pid());
        syscall_fork();
        syscall_fork();
        syscall_fork();
        syscall_fork();
        syscall_fork();
        syscall_fork();
        syscall_fork();

        printf("after so many forks: pid = %ld\n", syscall_get_pid());
    }
    else
    {
        printf("Parent process: pid = %ld, child pid = %d\n", syscall_get_pid(), pid);
        while (1)
            ;
    }
    return 0;
}
