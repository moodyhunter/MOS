// SPDX-License-Identifier: GPL-3.0-or-later

#include "libuserspace.h"

#include "mos/ksyscall/usermode.h"

extern int main(void);
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

void _start(void)
{
    int r = main();
    invoke_ksyscall_exit(r);
}
