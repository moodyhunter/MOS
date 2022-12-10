// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/memory.h"
#include "lib/string.h"
#include "libuserspace.h"
#include "mos/syscall/usermode.h"

static char buf[4 KB] = { 0 };

static void thread_work(void *arg)
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

    syscall_spawn("/programs/locks", 0, NULL);

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
        uintptr_t heap_top = syscall_heap_control(HEAP_GET_TOP, 0);
        printf("Child top: " PTR_FMT "\n", heap_top);
        uintptr_t new_top = syscall_heap_control(HEAP_SET_TOP, heap_top + 16 KB);
        printf("Child new top: " PTR_FMT "\n", new_top);

        char *ptr = (char *) new_top - 1;
        *ptr = 0x42;
        u32 pid_new_forked = syscall_fork();
        if (pid_new_forked == 0)
        {
            printf("Child process of child process\n");
            syscall_exit(0);
        }
        else
        {
            printf("Parent process of child process\n");
        }
    }
    else
    {
        printf("Parent process\n");
    }

    pid_t parent = syscall_get_parent_pid();
    printf("Parent PID: %d\n", parent);

    // get heap address
    uintptr_t heap = syscall_heap_control(HEAP_GET_TOP, 0);
    printf("Heap base: " PTR_FMT "\n", heap);

    // grow the heap by 16 KB
    uintptr_t new_heap = syscall_heap_control(HEAP_GROW_PAGES, 4); // in pages

    printf("New heap top: " PTR_FMT "\n", new_heap);

    // write to the heap
    char *ptr = (char *) new_heap - 1;
    *ptr = 0x42;

    char *data = malloc(50);
    strcpy(data, "Hello world!");
    printf("Data: %s\n", data);

    while (1)
        ;

    return 0;
}
