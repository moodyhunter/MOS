// SPDX-License-Identifier: GPL-3.0-or-later

#include "libuserspace.h"

#include "lib/stdio.h"
#include "lib/string.h"
#include "mos/platform/platform.h"
#include "mos/syscall/usermode.h"

#include <stdarg.h>

typedef struct thread_start_args
{
    thread_entry_t entry;
    void *arg;
} thread_start_args_t;

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
    extern int main(void);
    int r = main();
    syscall_exit(r);
}

void _thread_start(void *arg)
{
    thread_start_args_t *args = (thread_start_args_t *) arg;
    args->entry(args->arg);
    syscall_thread_exit();
}

void dvprintf(int fd, const char *fmt, va_list ap)
{
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    syscall_io_write(fd, buf, strlen(buf), 0);
}

void dprintf(int fd, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    dvprintf(fd, fmt, ap);
    va_end(ap);
}

void printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    dvprintf(stdout, fmt, ap);
    va_end(ap);
}

// TODO support 1) malloc? or 2) thread-local storage?
static thread_start_args_t thread_start_args;

void start_thread(const char *name, thread_entry_t entry, void *arg)
{
    thread_start_args = (thread_start_args_t){ .entry = entry, .arg = arg };
    syscall_create_thread(name, _thread_start, &thread_start_args);
}
