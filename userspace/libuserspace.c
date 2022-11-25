// SPDX-License-Identifier: GPL-3.0-or-later

#include "libuserspace.h"

#include "lib/string.h"
#include "mos/platform/platform.h"
#include "mos/syscall/usermode.h"

typedef struct thread_start_args
{
    thread_entry_t entry;
    void *arg;
} thread_start_args_t;

extern int main(void);
u64 __stack_chk_guard = 0;

noreturn void __stack_chk_fail(void)
{
    syscall_exit(-1);
    while (1)
        ;
}

void __stack_chk_fail_local(void)
{
    __stack_chk_fail();
}

void _start(void)
{
    int r = main();
    syscall_exit(r);
}

void _thread_start(void *arg)
{
    thread_start_args_t *args = (thread_start_args_t *) arg;
    args->entry(args->arg);
    syscall_thread_exit();
}

void printf(const char *fmt, ...)
{
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    syscall_io_write(stdout, buf, strlen(buf), 0);
}

// TODO support 1) malloc? or 2) thread-local storage?
static thread_start_args_t thread_start_args;

void start_thread(const char *name, thread_entry_t entry, void *arg)
{
    thread_start_args = (thread_start_args_t){ .entry = entry, .arg = arg };
    syscall_create_thread(name, _thread_start, &thread_start_args);
}
