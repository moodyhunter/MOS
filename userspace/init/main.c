// SPDX-License-Identifier: GPL-3.0-or-later

#include "lib/stdio.h"
#include "libuserspace.h"
#include "mos/filesystem/filesystem.h"
#include "mos/ksyscall/usermode.h"
#include "mos/mos_global.h"
#include "mos/types.h"

#define STRINGIFY(x) #x
#define TOSTRING(x)  STRINGIFY(x)

#define print(msg) print_impl(TOSTRING(__LINE__) ": msg: " msg)

void print_impl(const char *str)
{
    invoke_ksyscall_io_write(stdout, str, strlen(str), 0);
    invoke_ksyscall_io_write(stdout, "\n", 1, 0);
}

void print_err(const char *str)
{
    invoke_ksyscall_io_write(stderr, str, strlen(str), 0);
    invoke_ksyscall_io_write(stderr, "\n", 1, 0);
}

void print_int(int i)
{
    switch (i)
    {
        case 0: print("0"); break;
        case 1: print("1"); break;
        case 2: print("2"); break;
        case 3: print("3"); break;
        case 4: print("4"); break;
        case 5: print("5"); break;
        case 6: print("6"); break;
        case 7: print("7"); break;
        case 8: print("8"); break;
        case 9: print("9"); break;
        case 10: print("10"); break;
    }
}

static char buf[2 KB] = { 0 };

int main(void)
{
    volatile int mm = 11;
    int fd = invoke_ksyscall_file_open("/assets/msg.txt", FILE_OPEN_READ);
    if (fd < 0)
        print_err("Failed to open /assets/msg.txt");
    else
    {
        size_t read = invoke_ksyscall_io_read(fd, buf, 512, 0);
        print("\n");
        invoke_ksyscall_io_write(stdout, buf, read, 0);
        invoke_ksyscall_io_close(fd);
    }

    pid_t my_pid = invoke_ksyscall_get_pid();
    print_int(my_pid);

    pid_t ping_pid = invoke_ksyscall_spawn("/programs/kmsg-ping", 0, NULL);
    pid_t pong_pid = invoke_ksyscall_spawn("/programs/kmsg-pong", 0, NULL);
    print_int(ping_pid);
    print_int(pong_pid);

    my_pid = invoke_ksyscall_fork();

    if (mm != 11)
        invoke_ksyscall_panic();

    sprintf(buf, "Hello from pid %d", my_pid);
    print_impl(buf);

    if (my_pid == 0)
    {
        print("Child process");
    }
    else
    {
        print("Parent process");
    }

    pid_t parent = invoke_ksyscall_get_parent_pid();
    print_int(parent);

    while (1)
        invoke_ksyscall_yield_cpu();

    return 0;
}
