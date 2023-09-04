// SPDX-License-Identifier: GPL-3.0-or-later

#include "struct_file.h"

#include <fcntl.h>
#include <mos/filesystem/fs_types.h>
#include <mos/io/io_types.h>
#include <mos/syscall/usermode.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>

struct _FILE __stdin = { .fd = 0 };
struct _FILE __stdout = { .fd = 1 };
struct _FILE __stderr = { .fd = 2 };

FILE *stdin = &__stdin;
FILE *stdout = &__stdout;
FILE *stderr = &__stderr;

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

int vfprintf(FILE *stream, const char *fmt, va_list ap)
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

int fputs(const char *__restrict s, FILE *__restrict file)
{
    return syscall_io_write(file->fd, s, strlen(s));
}

int fputc(int c, FILE *file)
{
    char ch = c;
    return syscall_io_write(file->fd, &ch, 1);
}

int fgetc(FILE *file)
{
    char c;
    syscall_io_read(file->fd, &c, 1);
    return c;
}

size_t fread(void *__restrict ptr, size_t size, size_t nmemb, FILE *__restrict stream)
{
    return syscall_io_read(stream->fd, ptr, size * nmemb) / size;
}

size_t fwrite(const void *__restrict ptr, size_t size, size_t nmemb, FILE *__restrict stream)
{
    return syscall_io_write(stream->fd, ptr, size * nmemb) / size;
}

int fseek(FILE *stream, long offset, io_seek_whence_t whence)
{
    return syscall_io_seek(stream->fd, offset, whence);
}

off_t ftell(FILE *stream)
{
    return syscall_io_tell(stream->fd);
}

FILE *fopen(const char *path, const char *mode)
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

    fd_t fd = open(path, flags);
    if (fd < 0)
        return NULL;

    FILE *file = malloc(sizeof(FILE));
    file->fd = fd;
    return file;
}

int fclose(FILE *stream)
{
    int ret = syscall_io_close(stream->fd);
    free(stream);
    return ret;
}
