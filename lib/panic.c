// SPDX-License-Identifier: GPL-3.0-or-later

#include "bug.h"
#include "drivers/screen.h"

noreturn void _kpanic_impl(const char *msg, const char *func, const char *file, const char *line)
{
    // TODO: switch to printk once it's implemented
    screen_set_color(White, Red);
    screen_print_string("!!!!!!!!!!!!!!!!!!!!!!!!\n");
    screen_print_string("!!!!! KERNEL PANIC !!!!!\n");
    screen_print_string("!!!!!!!!!!!!!!!!!!!!!!!!\n");
    screen_print_string("\n");
    screen_set_color(Red, Black);
    screen_print_string(msg);
    screen_print_string("\n");
    screen_print_string("  in function: ");
    screen_print_string(func);
    screen_print_string("\n");
    screen_print_string("  at file: ");
    screen_print_string(file);
    screen_print_string(":");
    screen_print_string(line);
    screen_print_string("\n");

    while (1)
        ;
}

void _kwarn_impl(const char *msg, const char *func, const char *file, const char *line)
{
    // TODO: switch to printk once it's implemented
    screen_print_string("\n");
    screen_set_color(White, Brown);
    screen_print_string("warning: ");
    screen_print_string(msg);
    screen_set_color(Brown, Black);
    screen_print_string("\n");
    screen_print_string("  in function: ");
    screen_print_string(func);
    screen_print_string("\n");
    screen_print_string("  at file: ");
    screen_print_string(file);
    screen_print_string(":");
    screen_print_string(line);
    screen_print_string("\n");
    screen_set_color(LightGray, Black);
}
