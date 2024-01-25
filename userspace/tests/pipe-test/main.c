// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos_signal.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>
#include <signal.h>

void badbadbad(void)
{
    puts("badbadbad");
    abort();
}

static const char *data = "Hello, world!";

static void sigpipe_handler(signal_t)
{
    puts("SIGPIPE received");
}

void reader(int fd)
{
    char buf[320] = { 0 };
    size_t bytes_read = syscall_io_read(fd, buf, sizeof(buf));
    if (IS_ERR_VALUE(bytes_read))
        badbadbad();

    printf("reader: read %zu bytes: %.*s\n", bytes_read, (int) bytes_read, buf);

    if (bytes_read != strlen(data) + 1)
        badbadbad();

    printf("read done\n");
}

void writer(int fd)
{
    // syscall_clock_msleep(1000);
    size_t bytes_written = syscall_io_write(fd, data, strlen(data) + 1);
    if (IS_ERR_VALUE(bytes_written))
    {
        if (bytes_written != (size_t) -EPIPE)
            badbadbad(); // unexpected error
    }
    else
    {
        printf("writer: wrote %zu bytes\n", bytes_written);
    }
}

int main(void)
{
    puts("MOS pipe(2) test.");
    register_signal_handler(SIGPIPE, sigpipe_handler);

    fd_t r, w;
    long result = syscall_pipe(&r, &w, FD_FLAGS_NONE);
    if (IS_ERR_VALUE(result))
    {
        printf("pipe(2) failed: %ld\n", result);
        return 1;
    }

    if (syscall_fork() == 0)
    {
        // child = writer
        syscall_io_close(r);

        writer(w), syscall_io_close(w);
    }
    else
    {
        // parent = reader
        syscall_io_close(w);

        reader(r), syscall_io_close(r);
    }

    return 0;
}
