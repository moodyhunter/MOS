// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/mos_global.h>
#include <stdio.h>

int main(void)
{
    puts("Lab 4 Test Utility - Part 1");

#if SYSCALL_DEFINED(dump_pagetable) // expand to nothing if the syscall is not defined
    syscall_dump_pagetable();
    puts("Done");

    if (syscall_get_pid() == 1)
        syscall_poweroff(false, MOS_FOURCC('G', 'B', 'y', 'e'));

    return 0;
#else
    puts("SYSCALL_dump_pagetable is not defined");
    return 1;
#endif
}
