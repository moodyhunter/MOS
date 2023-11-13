// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/syscall/usermode.h>
#include <mos_stdio.h>
#include <mos_stdlib.h>
#include <mos_string.h>
#include <unistd.h>

static fd_t server_fd;
static const char data[44] = "The quick brown fox jumps over the lazy dog";

static void thread_main(void *arg)
{
    const fd_t client_fd = (fd_t) (long) arg;

    char buf[150];
    const size_t bufsize = sprintf(buf, "welcome to the server, client fd %d. '%s'!", client_fd, data);

    syscall_io_write(client_fd, &bufsize, sizeof(bufsize));
    const size_t written = syscall_io_write(client_fd, buf, bufsize);
    if (written != bufsize)
        puts("server: failed to write to ipc channel");

    size_t readbufsize = 0;
    const size_t read = syscall_io_read(client_fd, &readbufsize, sizeof(readbufsize));

    if (read != sizeof(readbufsize))
    {
        puts("server: failed to read from ipc channel");
        return;
    }

    char *readbuf = malloc(readbufsize);
    const size_t read_size = syscall_io_read(client_fd, readbuf, readbufsize);
    if (read_size != readbufsize)
    {
        puts("server: failed to read from ipc channel");
        return;
    }

    printf("server: received '%.*s' from client %d\n", (int) read_size, readbuf, client_fd);

    syscall_io_close(client_fd);
    syscall_io_close(server_fd);
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
    const char *pond_argv[] = { pong_path, ipc_name, NULL };

    spawn(pong_path, pond_argv);

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
        start_thread("ipc-server child", thread_main, (void *) (long) client_fd);
    }

    return 0;
}
