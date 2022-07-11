// SPDX-License-Identifier: GPL-3.0-or-later

#include "stdio.h"

void start_kernel(void)
{
    const char *kernel_name = "Hello from the MOS Kernel!";
    print_string(kernel_name);

    float p = 0.0033314 + 0.0033314664;

    while (1)
        ;
}
