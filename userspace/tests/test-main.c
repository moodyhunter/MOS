// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/stdio.h"
#include "mos/syscall/usermode.h"

struct
{
    const char *name;
    const char *executable;
} const tests[] = {
    { "fork", "/tests/fork-test" },
    { "cxx", "/tests/cxx_test" },
    { "rpc", "/tests/rpc-test" },
    { 0 },
};

int main(int argc, char **argv)
{
    printf("MOS Userspace Test Suite\n");
    printf("Invoked with %d arguments:\n", argc);
    for (int i = 0; i < argc; i++)
        printf("  %d: %s\n", i, argv[i]);

    printf("\n");

    for (int i = 0; tests[i].name; i++)
    {
        printf("Running test %s (%s)... ", tests[i].name, tests[i].executable);
        pid_t ret = syscall_spawn(tests[i].executable, 0, NULL);
        if (ret < 0)
        {
            printf("FAILED: cannot spawn: %ld\n", ret);
            continue;
        }

        syscall_wait_for_process(ret);
        printf("OK\n");
    }

    pid_t p = syscall_spawn("/programs/shutdown", 0, NULL);
    syscall_wait_for_process(p);
    return 0;
}
