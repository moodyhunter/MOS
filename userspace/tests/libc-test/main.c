// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>

int main(int argc, const char *argv[])
{
    puts("Hello, world from mlibc-based userspace");

    for (int i = 0; i < argc; i++)
        printf("argv[%d] = %s\n", i, argv[i]);

    return 0;
}
