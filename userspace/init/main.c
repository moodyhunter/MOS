// SPDX-License-Identifier: GPL-3.0-or-later

#include "libuserspace.h"
#include "mos/filesystem/filesystem.h"
#include "mos/ksyscall/usermode.h"
#include "mos/mos_global.h"
#include "mos/types.h"

void print(const char *str)
{
    invoke_ksyscall_io_write(stdout, str, strlen(str), 0);
}

void print_err(const char *str)
{
    invoke_ksyscall_io_write(stderr, str, strlen(str), 0);
}

static char buf[2 KB] = { 0 };

int main(void)
{
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
    MOS_UNUSED(my_pid);

    pid_t pid = invoke_ksyscall_fork();

    if (pid == 0)
    {
        print("Child process\n");
    }
    else
    {
        print("Parent process\n");
    }

    while (1)
        invoke_ksyscall_yield_cpu();

    return 0;
}
