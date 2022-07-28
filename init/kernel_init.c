// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/boot/multiboot.h"
#include "mos/bug.h"
#include "mos/drivers/port.h"
#include "mos/drivers/screen.h"
#include "mos/kconfig.h"
#include "mos/stdio.h"
#include "mos/stdlib.h"
#include "mos/string.h"

#ifdef MOS_KERNEL_RUN_TESTS
extern void test_engine_run_tests();
#endif

void print_hex(u32 value)
{
    screen_print_string("0");
    screen_print_string("x");
    u32 i = 0;
    for (i = 0; i < 8; i++)
    {
        u8 nibble = (value >> (28 - i * 4)) & 0xF;
        if (nibble < 10)
            screen_print_char('0' + nibble);
        else
            screen_print_char('A' + nibble - 10);
    }
}

int printf(const char *format, ...)
{
    char buf[256] = { 0 };
    va_list args;
    va_start(args, format);

    int ret = vsnprintf(buf, 0, format, args);

    va_end(args);

    screen_print_string(buf);
    return ret;
}

void start_kernel(u32 magic, multiboot_info_t *addr)
{
    screen_init();
    screen_set_cursor_pos(0, 0);
    screen_disable_cursor();

    printf("'%#o'\n", 0);

    screen_set_color(LightGray, Black);
    printf("\n");
    screen_print_string("Multiboot Magic: ");
    print_hex((u32) magic);
    screen_print_string("\n");

    screen_print_string("cmdline = (");
    print_hex((u32) addr);
    screen_print_string("):");
    screen_print_string(addr->cmdline);

    printf("\n");
    printf("\n");

    screen_set_color(Yellow, Black);
    printf("MOS starting...\n");
    printf("Kernel:          '%s'\n", MOS_KERNEL_VERSION);
    printf("Revision:        '%s'\n", MOS_KERNEL_REVISION);
    printf("Builtin cmdline: '%s'\n", MOS_KERNEL_BUILTIN_CMDLINE);

    printf("\n");
    screen_set_color(Black, Green);

    warning("V2Ray 4.45.2 started");

#ifdef MOS_KERNEL_RUN_TESTS
    test_engine_run_tests();
#endif
    while (1)
        ;
}
