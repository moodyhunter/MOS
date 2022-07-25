// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/boot/multiboot.h"
#include "mos/bug.h"
#include "mos/drivers/port.h"
#include "mos/drivers/screen.h"
#include "mos/kconfig.h"
#include "mos/stdio.h"
#include "mos/stdlib.h"
#include "mos/string.h"

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

void printf_test()
{
    printf(" 1: %d\n", 1);
    printf(" 0: %d\n", 0);
    printf("-1: %d\n", -1);

    printf("padded 1: '%10d'\n", 1);
    printf("padded 0: '%10d'\n", 0);
    printf("padded-1: '%10d'\n", -1);

    printf("precise  1: '%.5d'\n", 1);
    printf("precise  0: '%.5d'\n", 0);
    printf("precise -1: '%.5d'\n", -1);

    printf("0 padded 1: '%010d'\n", 1);
    printf("0 padded 0: '%010d'\n", 0);
    printf("0 padded-1: '%010d'\n", -1);

    printf("INT_MAX: %d\n", INT_MAX);
    printf("INT_MIN: %d\n", INT_MIN);
    printf("UINT_MAX: %u\n", UINT_MAX);
    printf("UINT_MIN: %u\n", 0U);

    printf("CHAR_MAX: %c, or as integer: %d\n", CHAR_MAX, CHAR_MAX);
    printf("CHAR_MIN: %c, or as integer: %d\n", CHAR_MIN, CHAR_MIN);

    printf("UCHAR_MAX: %c\n", UCHAR_MAX);
    printf("UCHAR_MIN: %c\n", (uchar) 0);

    printf("LONG_MAX: %ld\n", LONG_MAX);
    printf("LONG_MIN: %ld\n", LONG_MIN);
    printf("ULONG_MAX: %lu\n", ULONG_MAX);
    printf("ULONG_MIN: %lu\n", 0LU);

    // void *ptr = (void *) 0x12345678;
    // printf("ptr: %p\n", ptr);
    // printf("ptr: %x\n", ptr);
    // printf("ptr: %X\n", ptr);
    // printf("ptr: %lx\n", ptr);
    // printf("ptr: %lX\n", ptr);
}

void start_kernel(u32 magic, multiboot_info_t *addr)
{
    screen_init();
    screen_set_cursor_pos(0, 0);
    screen_disable_cursor();

    screen_set_color(LightGray, Black);
    printf_test();

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

    printf("%s test done.\n", "printf");
    if (strcmp(addr->cmdline, "mos_multiboot.bin exit clean") == 0)
        port_outw(0x604, 0x2000);
    else
        port_outl(0xf4, 0);
}
