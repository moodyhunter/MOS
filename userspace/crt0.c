// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/ksyscall/usermode.h"

extern int main(void);

void _start(void)
{
    int r = main();
    invoke_ksyscall_exit(r);
}
