// SPDX-License-Identifier: GPL-3.0-or-later

#include "libuserspace.h"
#include "mos/syscall/usermode.h"

static char buf[4 KB] = { 0 };

void thread_work(void *arg)
{
    int *value = (int *) arg;
    pid_t process = syscall_get_pid();
    printf("Thread started, value = %d, from process %d\n", *value, process);
}

static int value = 0;

int main(void)
{
    printf("\n");
    int fd = syscall_file_open("/assets/msg.txt", FILE_OPEN_READ);
    if (fd < 0)
        printf("Failed to open /assets/msg.txt\n");
    else
    {
        size_t read = syscall_io_read(fd, buf, 512, 0);
        syscall_io_write(stdout, buf, read, 0);
        syscall_io_close(fd);
    }

    char buf[256] = { 0 };

    long read = syscall_io_read(stdin, buf, 256, 0);
    if (read > 0)
    {
        printf("Read %d bytes from stdin", read);
        syscall_io_write(stdout, buf, read, 0);
    }

    pid_t my_pid = syscall_get_pid();
    printf("My PID: %d\n", my_pid);

    value = 3456787;

    pid_t ping_pid = syscall_spawn("/programs/kmsg-ping", 0, NULL);
    pid_t pong_pid = syscall_spawn("/programs/kmsg-pong", 0, NULL);
    printf("ping pid: %d\n", ping_pid);
    printf("pong pid: %d\n", pong_pid);

    start_thread("worker", thread_work, &value);

    my_pid = syscall_fork();

    if (my_pid == 0)
    {
        printf("Child process\n");
        syscall_exit(0);
    }
    else
    {
        printf("Parent process\n");
    }

    pid_t parent = syscall_get_parent_pid();
    printf("Parent PID: %d\n", parent);

    while (1)
        ;

    return 0;
}
