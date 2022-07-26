// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/panic.h"

#include "mos/bug.h"
#include "mos/drivers/screen.h"

// kpanic_handler is called when a panic occurs.

static kmsg_handler_t kpanic_handler = NULL;
static kmsg_handler_t kwarn_handler = NULL;

void kpanic_handler_set(__attr_noreturn kmsg_handler_t handler)
{
    kpanic_handler = handler;
}

void kpanic_handler_remove()
{
    kpanic_handler = NULL;
}

void kwarn_handler_set(kmsg_handler_t handler)
{
    kwarn_handler = handler;
}

void kwarn_handler_remove()
{
    kwarn_handler = NULL;
}

void _kpanic_impl(const char *msg, const char *func, const char *file, const char *line)
{
    if (kpanic_handler)
    {
        kpanic_handler(msg, func, file, line);
        MOS_UNREACHABLE();
    }

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
    if (kwarn_handler)
    {
        kwarn_handler(msg, func, file, line);
        return;
    }
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
