// SPDX-License-Identifier: GPL-3.0-or-later

#include "stdio.h"

void start_kernel(void)
{
    const char *kernel_name = "Hello from the MOS kernel!";
    print_string(kernel_name);

    while (1)
        ;
}
