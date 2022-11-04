// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/stdio.h"
#include "libuserspace.h"
#include "mos/platform/platform.h"
#include "mos/syscall/usermode.h"

void printf(const char *fmt, ...)
{
    char buf[4096];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    syscall_io_write(stdout, buf, strlen(buf), 0);
}

static char buf[4 KB] = { 0 };

void thread_work(void *arg)
{
    int *value = (int *) arg;
    pid_t process = syscall_get_pid();
    printf("Thread started, value = %d, from process %d\n", *value, process);
    // TODO: implement thread termination
    while (1)
        ;
}

static int value = 0;

int main(void)
{
    int fd = syscall_file_open("/assets/msg.txt", FILE_OPEN_READ);
    if (fd < 0)
        printf("Failed to open /assets/msg.txt\n");
    else
    {
        size_t read = syscall_io_read(fd, buf, 512, 0);
        printf("\n");
        syscall_io_write(stdout, buf, read, 0);
        syscall_io_close(fd);
    }

    pid_t my_pid = syscall_get_pid();
    printf("My PID: %d\n", my_pid);

    value = 3456787;

    pid_t ping_pid = syscall_spawn("/programs/kmsg-ping", 0, NULL);
    pid_t pong_pid = syscall_spawn("/programs/kmsg-pong", 0, NULL);
    printf("ping pid: %d\n", ping_pid);
    printf("pong pid: %d\n", pong_pid);

    syscall_create_thread("worker", thread_work, &value);

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
