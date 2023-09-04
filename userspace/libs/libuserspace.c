// SPDX-License-Identifier: GPL-3.0-or-later

#include "liballoc.h"
#include "struct_file.h"

#include <mos/syscall/usermode.h>
#include <mos_signal.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <stdarg.h>

typedef struct thread_start_args
{
    thread_entry_t entry;
    void *arg;
} thread_start_args_t;

u64 __stack_chk_guard = 0xdeadbeefdeadbeef;

noreturn void __stack_chk_fail(void)
{
    fputs("stack smashing detected...", stdout);
    syscall_exit(-1);
    while (1)
        ;
}

void __stack_chk_fail_local(void)
{
    __stack_chk_fail();
}

static void invoke_init(void)
{
    typedef void (*func_ptr)(void);
    extern func_ptr __init_array_start[], __init_array_end;

    // if anything goes wrong here, it must be a bug in the ELF loader
    for (func_ptr *func = __init_array_start; func != &__init_array_end; func++)
        (*func)();
}

static void __attribute__((constructor)) __liballoc_userspace_init(void)
{
    liballoc_init();
}

void _start(size_t argc, char **argv)
{
    extern int main(int argc, char **argv);
    extern void __cxa_finalize(void *d);

    invoke_init();
    int r = main(argc, argv);
    __cxa_finalize(NULL);
    syscall_exit(r);
}

void fatal_abort(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    abort();
    while (1)
        ;
}

static void thread_start(void *_arg)
{
    thread_entry_t entry = ((thread_start_args_t *) _arg)->entry;
    void *entry_arg = ((thread_start_args_t *) _arg)->arg;
    free(_arg);
    entry(entry_arg);
    syscall_thread_exit();
}

tid_t start_thread(const char *name, thread_entry_t entry, void *arg)
{
    thread_start_args_t *thread_start_args = malloc(sizeof(thread_start_args_t));
    thread_start_args->entry = entry;
    thread_start_args->arg = arg;
    return syscall_create_thread(name, thread_start, thread_start_args);
}

noreturn void abort()
{
    raise(SIGABRT);
    exit(-1);
}
