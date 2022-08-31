// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ksyscall/usermode.h"
#include "mos/types.h"

void _start(uintptr_t arg)
{
    MOS_UNUSED(arg);
    invoke_ksyscall_io_write(1, "Hello, world! MOS userspace '/bin/init'", 39, 0);

    while (true)
        ;
}
