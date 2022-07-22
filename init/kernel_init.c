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
            screen_print_char('0' + nibble);
        }
        else
        {
            screen_print_char('A' + nibble - 10);
        }
    }
}

void start_kernel(u32 magic, multiboot_info_t *addr)
{
    screen_init();
    screen_set_cursor_pos(0, 0);
    screen_disable_cursor();

    print_hex((u32) magic);
    screen_print_string("\n");
    print_hex((u32) addr);
    screen_print_string("\n");
    screen_print_string(addr->cmdline);

    screen_set_color(Yellow, Black);
    screen_print_string_at("Kernel: ", 0, 10);
    screen_print_string_at("Revision: ", 0, 11);
    screen_print_string_at("Builtin cmdline: ", 0, 12);

    screen_set_color(Cyan, Black);
    screen_print_string_at(MOS_KERNEL_VERSION, 20, 10);
    screen_print_string_at(MOS_KERNEL_REVISION, 20, 11);
    screen_print_string_at(MOS_KERNEL_BUILTIN_CMDLINE, 20, 12);

    screen_set_color(Green, Black);
    screen_print_string_at("Long live MOS!", 0, 14);

    while (1)
        ;
}
