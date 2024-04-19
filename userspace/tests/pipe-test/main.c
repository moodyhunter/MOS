// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mos_global.h>
#include <mos/tasks/signal_types.h>
#include <mos/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
    const ssize_t bytes_read = read(fd, buf, sizeof(buf));
    if (bytes_read == -1)
        perror("reader: read failed"), exit(1);

    printf("reader: read %zu bytes: '%.*s'\n", bytes_read, (int) bytes_read, buf);

    if (bytes_read != (ssize_t) strlen(data) + 1)
        badbadbad();

    printf("read done\n");
}

void writer(int fd)
{
    // syscall_clock_msleep(1000);
    size_t bytes_written = write(fd, data, strlen(data) + 1);
    if (IS_ERR_VALUE(bytes_written))
    {
        if (bytes_written != (size_t) -EPIPE)
            puts("writer: write failed"), exit(1);
    }
    else
    {
        printf("writer: wrote %zu bytes\n", bytes_written);
    }
}

int main(void)
{
    puts("MOS pipe(2) test.");
    signal(SIGPIPE, sigpipe_handler);

    fd_t fds[2];
    if (pipe(fds))
    {
        perror("pipe(2) failed");
        return 1;
    }

    const fd_t r = fds[0], w = fds[1];

    if (fork() == 0)
    {
        // child = writer
        close(r);
        writer(w);
        close(w);
    }
    else
    {
        // parent = reader
        close(w);
        reader(r);
        close(r);
    }

    return 0;
}
