// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ksyscall/usermode.h"
#include "mos/types.h"

#define stdin  0
#define stdout 1
#define stderr 2

void _start(void)
{
    const char *x = "Hello, world! MOS userspace '/bin/init'";
    invoke_ksyscall_io_write(stdout, x, 39, 0);

    while (true)
        ;
}
