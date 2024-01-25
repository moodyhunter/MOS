// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/syscall/usermode.h>
#include <pthread.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static fd_t server_fd;
static const char data[44] = "The quick brown fox jumps over the lazy dog";

static void *thread_main(void *arg)
{
    const fd_t client_fd = (fd_t) (long) arg;

    char buf[150];
    const size_t bufsize = sprintf(buf, "welcome to the server, client fd %d. '%s'!", client_fd, data);

    write(client_fd, &bufsize, sizeof(bufsize));
    const size_t written = write(client_fd, buf, bufsize);
    if (written != bufsize)
        puts("server: failed to write to ipc channel");

    size_t readbufsize = 0;
    const size_t readn = read(client_fd, &readbufsize, sizeof(readbufsize));

    if (readn != sizeof(readbufsize))
    {
        puts("server: failed to read from ipc channel");
        return NULL;
    }

    char *readbuf = malloc(readbufsize);
    const size_t read_size = read(client_fd, readbuf, readbufsize);
    if (read_size != readbufsize)
    {
        puts("server: failed to read from ipc channel");
        return NULL;
    }

    printf("server: received '%.*s' from client %d\n", (int) read_size, readbuf, client_fd);

    close(client_fd);
    close(server_fd);

    return NULL;
}

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("usage: %s <ping-pong-ipc-channel>\n", argv[0]);
        puts("starts a server that accepts connections on the given ipc name.");
        return -1;
    }

    const char *ipc_name = argv[1];
    printf("server: ipc-name='%s'\n", ipc_name);

    const char *pong_path = "/initrd/tests/ipc-pong";
    const char *const pond_argv[] = { pong_path, ipc_name, NULL };

    pid_t p;
    posix_spawn(&p, pong_path, NULL, NULL, (char *const *) pond_argv, environ);

    server_fd = syscall_ipc_create(ipc_name, 32);
    if (server_fd < 0)
    {
        puts("failed to open ipc channel");
        return -1;
    }

    while (true)
    {
        const fd_t client_fd = syscall_ipc_accept(server_fd);
        if (client_fd == -ECONNABORTED)
        {
            puts("Server: server closed");
            return 0;
        }
        else if (client_fd < 0)
        {
            puts("failed to accept ipc channel");
            return -1;
        }

        printf("server: accepted fd %d\n", client_fd);

        pthread_t thread = { 0 };
        pthread_create(&thread, NULL, thread_main, (void *) (long) client_fd);
    }

    return 0;
}
