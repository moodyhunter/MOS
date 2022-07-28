// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/panic.h"

#include "mos/kernel.h"
#include "mos/stdio.h"

#include <stdarg.h>

static kmsg_handler_t kpanic_handler = NULL;
static kmsg_handler_t kwarn_handler = NULL;

void kwarn_handler_set(kmsg_handler_t handler)
{
    mos_warn("installing new warning handler: %p", (void *) handler);
    kwarn_handler = handler;
}

void kpanic_handler_set(__attr_noreturn kmsg_handler_t handler)
{
    mos_warn("installing new panic handler: %p", (void *) handler);
    kpanic_handler = handler;
}

void kwarn_handler_remove()
{
    mos_warn_no_handler("removing warning handler: %p", (void *) kwarn_handler);
    kwarn_handler = NULL;
}

void kpanic_handler_remove()
{
    mos_warn_no_handler("removing panic handler: %p", (void *) kpanic_handler);
    kpanic_handler = NULL;
}

// ! using the following 2 functions causes recurstion
#undef mos_warn
#undef mos_panic

void mos_kpanic(const char *func, u32 line, const char *fmt, ...)
{
    va_list args;
    if (kpanic_handler)
    {
        va_start(args, fmt);
        kpanic_handler(func, line, fmt, args);
        va_end(args);
    }

    char message[PRINTK_BUFFER_SIZE] = { 0 };
    va_start(args, fmt);
    vsnprintf(message, PRINTK_BUFFER_SIZE, fmt, args);
    va_end(args);

    mos_emerg_no_handler("!!!!!!!!!!!!!!!!!!!!!!!!");
    mos_emerg_no_handler("!!!!! KERNEL PANIC !!!!!");
    mos_emerg_no_handler("!!!!!!!!!!!!!!!!!!!!!!!!");
    mos_fatal_no_handler("%s", message);
    mos_emerg_no_handler("  in function: %s (line %u)", func, line);

    while (1)
        ;
}

void mos_kwarn(const char *func, u32 line, const char *fmt, ...)
{
    va_list args;
    if (kwarn_handler)
    {
        va_start(args, fmt);
        kwarn_handler(func, line, fmt, args);
        va_end(args);
        return;
    }

    char message[PRINTK_BUFFER_SIZE] = { 0 };
    va_start(args, fmt);
    vsnprintf(message, PRINTK_BUFFER_SIZE, fmt, args);
    va_end(args);

    mos_warn_no_handler("warning: %s", message);
    mos_warn_no_handler("  in function: %s (line %u)", func, line);
}
