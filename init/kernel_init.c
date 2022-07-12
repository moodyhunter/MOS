// SPDX-License-Identifier: GPL-3.0-or-later

#include "drivers/screen.h"
#include "kconfig.h"

void start_kernel(void)
{
    screen_clear();
    screen_cursor_disable();
    screen_print_string("Hello from the MOS Kernel!");
    screen_print_string_at(0, 2, "Kernel: ", Yellow, Black);
    screen_print_string_at(15, 2, KERNEL_VERSION, Cyan, Black);

    screen_print_string_at(0, 3, "Revision: ", Yellow, Black);
    screen_print_string_at(15, 3, KERNEL_REVISION, Cyan, Black);

    screen_print_string_at(0, 4, "Boot Type: ", Yellow, Black);
    screen_print_string_at(15, 4, KERNEL_BOOTMETHOD, Cyan, Black);

    screen_print_string_at(0, 7, "Long live MOS!", Green, Black);
}
