// SPDX-License-Identifier: GPL-3.0-or-later

#define __NO_START_FUNCTION
#include "mosapi.h"

#include <fcntl.h>
#include <mos/syscall/number.h>
#include <mos/syscall/usermode.h>
#include <mos_stdio.hpp>
#include <mos_stdlib.hpp>
#include <mos_string.hpp>
#include <stdarg.h>

struct _FILE
{
    int fd;
};

static struct _FILE __stdin = { .fd = 0 };
static struct _FILE __stdout = { .fd = 1 };
static struct _FILE __stderr = { .fd = 2 };

FILE *stdin = &__stdin;
FILE *stdout = &__stdout;
FILE *stderr = &__stderr;

typedef struct thread_start_args
{
    thread_entry_t entry;
    void *arg;
} thread_start_args_t;

u64 __stack_chk_guard = 0xdeadbeefdeadbeef;

[[noreturn]] void __stack_chk_fail(void)
{
    puts("stack smashing detected...");
    syscall_exit(-1);
    __builtin_unreachable();
}

void __stack_chk_fail_local(void)
{
    __stack_chk_fail();
}

void __printf(1, 2) fatal_abort(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vdprintf(stderr->fd, fmt, ap);
    va_end(ap);
    abort();
}

[[noreturn]] void abort(void)
{
    raise(SIGABRT);
    syscall_exit(-1);
}

fd_t open(const char *path, open_flags flags)
{
    if (path == NULL)
        return -1;

    return openat(AT_FDCWD, path, flags);
}

fd_t openat(fd_t fd, const char *path, open_flags flags)
{
    return syscall_vfs_openat(fd, path, flags);
}

int raise(signal_t sig)
{
    syscall_signal_thread(syscall_get_tid(), sig);
    return 0;
}

bool lstatat(int fd, const char *path, file_stat_t *buf)
{
    return syscall_vfs_fstatat(fd, path, buf, FSTATAT_NOFOLLOW) == 0;
}

bool chdir(const char *path)
{
    return syscall_vfs_chdirat(AT_FDCWD, path) == 0;
}

bool unlink(const char *path)
{
    return syscall_vfs_unlinkat(AT_FDCWD, path) == 0;
}

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int size = vdprintf(stdout->fd, fmt, ap);
    va_end(ap);
    return size;
}

int fprintf(FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int size = vdprintf(stream->fd, fmt, ap);
    va_end(ap);
    return size;
}

int vdprintf(int fd, const char *fmt, va_list ap)
{
    char buffer[256];
    int size = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    syscall_io_write(fd, buffer, size);
    return size;
}

int putchar(int c)
{
    char ch = c;
    syscall_io_write(stdout->fd, &ch, 1);
    return c;
}

int puts(const char *s)
{
    syscall_io_write(stdout->fd, s, strlen(s));
    return putchar('\n');
}

int fputs(const char *restrict s, FILE *restrict file)
{
    return syscall_io_write(file->fd, s, strlen(s));
}

size_t fwrite(const void *__restrict ptr, size_t size, size_t nmemb, FILE *__restrict stream)
{
    return syscall_io_write(stream->fd, ptr, size * nmemb) / size;
}
