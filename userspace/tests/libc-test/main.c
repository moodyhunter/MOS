// SPDX-License-Identifier: GPL-3.0-or-later

#include <stdio.h>

int main(int argc, const char *argv[])
{
    puts("Hello, world from mlibc-based userspace");

    for (int i = 0; i < argc; i++)
        printf("argv[%d] = %s\n", i, argv[i]);

    // dump all env vars
    puts("environment variables:");
    extern char **environ;
    char **env = environ;
    size_t nenv = 0;
    while (*env)
    {
        nenv++;
        printf("env: %s\n", *env);
        env++;
    }

    printf("total env vars: %zu\n", nenv);

    return 0;
}
