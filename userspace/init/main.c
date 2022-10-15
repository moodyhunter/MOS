// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/filesystem/filesystem.h"
#include "mos/ksyscall/usermode.h"
#include "mos/mos_global.h"
#include "mos/types.h"

#define stdin  0
#define stdout 1
#define stderr 2

u64 __stack_chk_guard = 0;

void noreturn __stack_chk_fail(void)
{
    invoke_ksyscall_panic();
    while (1)
        ;
}

void __stack_chk_fail_local(void)
{
    __stack_chk_fail();
}

int strlen(const char *str)
{
    int len = 0;
    while (str[len])
        len++;
    return len;
}

void print(const char *str)
{
    invoke_ksyscall_io_write(stdout, str, strlen(str), 0);
}

void print_err(const char *str)
{
    invoke_ksyscall_io_write(stderr, str, strlen(str), 0);
}

volatile char buf[256];
volatile char buf2[256] = "Hello, World!\n";
const char *x = "Hello, world! MOS userspace 'init': " __FILE__ "\n";

void _start(void)
{
    print(x);
    int fd = invoke_ksyscall_file_open("/assets/msg.txt", FILE_OPEN_READ);
    if (fd < 0)
    {
        print_err("Failed to open /assets/msg.txt");
    }
    else
    {
        char buf[512] = { 0 };
        invoke_ksyscall_io_read(fd, buf, 512, 0);
        print(buf);
        invoke_ksyscall_io_close(fd);
    }
    while (1)
        invoke_ksyscall_yield_cpu();
}
