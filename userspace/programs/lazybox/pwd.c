// SPDX-License-Identifier: GPL-3.0-or-later

#include "mosapi.h"

#include <mos/moslib_global.h>

int main(int, char **)
{
    char cwdbuf[4096] = { 0 };
    if (syscall_vfs_getcwd(cwdbuf, sizeof(cwdbuf)) < 0)
    {
        puts("pwd: getcwd failed");
        return 1;
    }

    puts(cwdbuf);
    return 0;
}
