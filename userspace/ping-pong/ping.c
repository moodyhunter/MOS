// SPDX-License-Identifier: GPL-2.0

#include "mos/ksyscall/usermode.h"

int main(void)
{
    invoke_ksyscall_io_write(1, "ping\n", 5, 0);
    return 0;
}
