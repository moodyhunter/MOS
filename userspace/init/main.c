// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ksyscall/usermode.h"
#include "mos/types.h"

#define stdin  0
#define stdout 1
#define stderr 2

int strlen(const char *str)
{
    int len = 0;
    while (str[len])
        len++;
    return len;
}

void print(const char *str)
{
    invoke_ksyscall_io_write(stdout, str, strlen(str), 0);
}

void _start(void)
{
    const char *x = "Hello, world! MOS userspace '/bin/init'";
    print(x);

    while (true)
    {
        print("yielding cpu...");
        invoke_ksyscall_yield_cpu();
        print("we are back!");
    }
}
