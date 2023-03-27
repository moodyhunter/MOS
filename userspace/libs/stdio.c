// SPDX-License-Identifier: GPL-3.0-or-later

#include "struct_file.h"

#include <mos/filesystem/fs_types.h>
#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct FILE __stdin = { .fd = 0 };
struct FILE __stdout = { .fd = 1 };
struct FILE __stderr = { .fd = 2 };

struct FILE *stdin = &__stdin;
struct FILE *stdout = &__stdout;
struct FILE *stderr = &__stderr;

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int size = vdprintf(stdout->fd, fmt, ap);
    va_end(ap);
    return size;
}

int fprintf(struct FILE *stream, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int size = vdprintf(stream->fd, fmt, ap);
    va_end(ap);
    return size;
}

int dprintf(int fd, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int size = vdprintf(fd, fmt, ap);
    va_end(ap);
    return size;
}

int vprintf(const char *fmt, va_list ap)
{
    return vdprintf(stdout->fd, fmt, ap);
}

int vfprintf(struct FILE *stream, const char *fmt, va_list ap)
{
    return vdprintf(stream->fd, fmt, ap);
}

int vdprintf(int fd, const char *fmt, va_list ap)
{
    char buffer[256];
    int size = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    syscall_io_write(fd, buffer, size);
    return size;
}

int getchar(void)
{
    char c;
    syscall_io_read(stdin->fd, &c, 1);
    return c;
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

int fputs(const char *__restrict s, struct FILE *__restrict file)
{
    return syscall_io_write(file->fd, s, strlen(s));
}

int fputc(int c, struct FILE *file)
{
    char ch = c;
    return syscall_io_write(file->fd, &ch, 1);
}

int fgetc(struct FILE *file)
{
    char c;
    syscall_io_read(file->fd, &c, 1);
    return c;
}

size_t fread(void *__restrict ptr, size_t size, size_t nmemb, struct FILE *__restrict stream)
{
    return syscall_io_read(stream->fd, ptr, size * nmemb) / size;
}

size_t fwrite(const void *__restrict ptr, size_t size, size_t nmemb, struct FILE *__restrict stream)
{
    return syscall_io_write(stream->fd, ptr, size * nmemb) / size;
}

struct FILE *fopen(const char *path, const char *mode)
{
    int flags = 0;
    if (strchr(mode, 'r'))
    {
        flags |= OPEN_READ;
    }

    if (strchr(mode, 'w'))
    {
        flags |= OPEN_WRITE;
    }

    if (strchr(mode, 'a'))
    {
        fputs("fopen: append mode is not supported yet\n", stderr);
        // flags |= OPEN_APPEND;
        return NULL;
    }

    if (strchr(mode, 'c'))
        flags |= OPEN_CREATE;

    if (strchr(mode, 't'))
    {
        fputs("fopen: truncate mode is not supported yet\n", stderr);
        // flags |= OPEN_TRUNCATE;
        return NULL;
    }

    if (strchr(mode, 'x'))
    {
        fputs("fopen: exclusive mode is not supported yet\n", stderr);
        // flags |= OPEN_EXCLUSIVE;
        return NULL;
    }

    fd_t fd = syscall_vfs_open(path, flags);
    if (fd < 0)
        return NULL;

    struct FILE *file = malloc(sizeof(struct FILE));
    file->fd = fd;
    return file;
}

int fclose(struct FILE *stream)
{
    int ret = syscall_io_close(stream->fd);
    free(stream);
    return ret;
}
