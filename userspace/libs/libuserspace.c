// SPDX-License-Identifier: GPL-3.0-or-later

#include "liballoc.h"
#include "mos_string.h"
#include "struct_file.h"

#include <mos/syscall/number.h>
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
char **environ = NULL;

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

void _start(size_t argc, char **argv, char **envp)
{
    extern int main(int argc, char **argv, char **envp);
    extern void __cxa_finalize(void *d);
    environ = envp;

    invoke_init();

// ensure that the stack is 16-byte aligned
#if defined(__x86_64__)
    __asm__ volatile("andq $-16, %rsp");
#else
#error "unsupported architecture"
#endif

    int r = main(argc, argv, envp);
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
    return syscall_create_thread(name, thread_start, thread_start_args, 0, NULL);
}

noreturn void abort()
{
    raise(SIGABRT);
    exit(-1);
}

pid_t spawn(const char *path, const char *const argv[])
{
    pid_t pid = syscall_fork();
    if (pid == 0)
    {
        syscall_execveat(FD_CWD, path, argv, (const char *const *) environ, 0);
        syscall_exit(-1);
    }

    return pid;
}

pid_t shell_execute(const char *command)
{
    char *dup = strdup(command);
    char *saveptr;
    char *driver_path = strtok_r(dup, " ", &saveptr);
    char *driver_args = strtok_r(NULL, " ", &saveptr);

    driver_path = string_trim(driver_path);
    driver_args = string_trim(driver_args);

    if (!driver_path)
        return false; // invalid options

    size_t driver_args_count = 1;
    const char **driver_argv = malloc(driver_args_count * sizeof(char *));
    driver_argv[0] = driver_path;

    if (driver_args)
    {
        char *saveptr;
        char *arg = strtok_r(driver_args, " ", &saveptr);
        while (arg)
        {
            driver_args_count++;
            driver_argv = realloc(driver_argv, driver_args_count * sizeof(char *));
            driver_argv[driver_args_count - 1] = arg;
            arg = strtok_r(NULL, " ", &saveptr);
        }
    }

    driver_argv = realloc(driver_argv, (driver_args_count + 1) * sizeof(char *));
    driver_argv[driver_args_count] = NULL;

    pid_t driver_pid = spawn(driver_path, driver_argv);
    if (driver_pid <= 0)
        return -1;

    for (size_t i = 0; i < driver_args_count; i++)
        free((void *) driver_argv[i]);

    free(driver_argv);
    free(dup);
    return driver_pid;
}
