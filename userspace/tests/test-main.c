// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/syscall/usermode.h>
#include <pthread.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

struct
{
    const char *name;
    const char *executable;
} const tests[] = {
    { "fork", "/initrd/tests/fork-test" },     //
    { "rpc", "/initrd/tests/rpc-test" },       //
    { "libc", "/initrd/tests/libc-test" },     //
    { "c++", "/initrd/tests/libstdc++-test" }, //
    { "rust", "/initrd/tests/rust-test" },     //
    { "pipe", "/initrd/tests/pipe-test" },     //
    { "signal", "/initrd/tests/signal" },      //
    { "syslog", "/initrd/tests/syslog-test" }, //
    { "memfd", "/initrd/tests/memfd-test" },   //
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
        const char *test_argv[] = { tests[i].name, NULL };
        pid_t ret;
        posix_spawn(&ret, tests[i].executable,
                    NULL, // file_actions
                    NULL, // attrp
                    (char *const *) test_argv, NULL);
        if (ret < 0)
        {
            printf("FAILED: cannot spawn: %d\n", ret);
            continue;
        }

        u32 status = 0;
        syscall_wait_for_process(ret, &status, 0);
        printf("Test %s exited with status %d\n", tests[i].name, status);
        printf("OK\n");
    }

    return 0;
}
