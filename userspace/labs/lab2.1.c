// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
int main(void)
{
    puts("Lab 2 Test Utility");

    // test fork
    pid_t pid = syscall_fork();
    if (pid == 0)
    {
        puts("I am the child");
        return 0;
    }
    else if (pid > 0)
    {
        puts("I am the parent, waiting for child");
        syscall_wait_for_process(pid);
    }
    else
    {
        puts("fork failed");
        return 1;
    }

    return 0;
}
