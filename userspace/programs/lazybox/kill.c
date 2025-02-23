// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

#include <mos/moslib_global.hpp>

int parse_string(const char *input)
{
    int value = 0;
    for (int i = 0; input[i] != '\0'; i++)
    {
        if (input[i] >= '0' && input[i] <= '9')
        {
            value = value * 10 + (input[i] - '0');
        }
        else
        {
            return -1;
        }
    }
    return value;
}

int main(int argc, char **argv)
{
    if (argc != 3)
    {
        printf("Usage: %s <pid> <signal>\n", argv[0]);
        return 1;
    }

    const int pid = parse_string(argv[1]);
    const int signal = parse_string(argv[2]);

    if (pid == -1 || signal == -1)
    {
        puts("invalid argument");
        return 1;
    }

    syscall_signal_process(pid, signal);
    puts("Signal sent");
    return 0;
}
