// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/syscall/usermode.h>
#include <mos_stdio.h>

struct
{
    const char *name;
    const char *executable;
} const tests[] = {
    { "fork", "/initrd/tests/fork-test" }, //
    { "cxx", "/initrd/tests/cxx_test" },   //
    { "rpc", "/initrd/tests/rpc-test" },   //
    { "libc", "/initrd/tests/libc-test" }, //
    { "rust", "/initrd/tests/rust-test" }, //
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
        printf("Running test %s (%s)... \n", tests[i].name, tests[i].executable);
        int test_argc = 1;
        const char *test_argv[] = { tests[i].name, NULL };
        pid_t ret = syscall_spawn(tests[i].executable, test_argc, test_argv);
        if (ret < 0)
        {
            printf("FAILED: cannot spawn: %d\n", ret);
            continue;
        }

        syscall_wait_for_process(ret);
        printf("OK\n");
    }

    return 0;
}
