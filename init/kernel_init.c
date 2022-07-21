// SPDX-License-Identifier: GPL-3.0-or-later

#include "boot/multiboot.h"
#include "drivers/screen.h"
#include "kconfig.h"

void print_hex(u32 value)
{
    screen_print_string("0");
    screen_print_string("x");
    u32 i = 0;
    for (i = 0; i < 8; i++)
    {
        u8 nibble = (value >> (28 - i * 4)) & 0xF;
        if (nibble < 10)
        {
            screen_putchar('0' + nibble, White, Black);
        }
        else
        {
            screen_putchar('A' + nibble - 10, White, Black);
        }
    }
}

void start_kernel(u32 magic, multiboot_info_t *addr)
{
    screen_clear();
    screen_cursor_disable();

    screen_print_string(addr->cmdline);
    screen_print_string("\n");
    print_hex((u32) magic);
    screen_print_string("\n");
    print_hex((u32) addr);
    screen_print_string("\n");
    print_hex((u32) addr->cmdline);

    screen_print_string_at(0, 10, "Kernel: ", Yellow, Black);
    screen_print_string_at(15, 10, KERNEL_VERSION, Cyan, Black);

    screen_print_string_at(0, 11, "Revision: ", Yellow, Black);
    screen_print_string_at(15, 11, KERNEL_REVISION, Cyan, Black);

    screen_print_string_at(0, 12, "Boot Type: ", Yellow, Black);
    screen_print_string_at(15, 12, "KERNEL_BOOTMETHOD", Cyan, Black);

    screen_print_string_at(0, 14, "Long live MOS!", Green, Black);

    while (1)
        ;
}
