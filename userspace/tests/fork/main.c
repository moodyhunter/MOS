// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    setbuf(stdout, NULL);
    for (int n = 0; n < 10; n++)
    {
        pid_t pid = fork();
        if (pid)
        {
            printf("pid %d, fork returned %d\n", getpid(), pid);
            fflush(stdout);
        }
    }
    return 0;
}
