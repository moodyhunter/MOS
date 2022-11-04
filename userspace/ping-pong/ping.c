// SPDX-License-Identifier: GPL-2.0

#include "mos/syscall/usermode.h"

int main(void)
{
    syscall_io_write(1, "ping\n", 5, 0);
    return 0;
}
